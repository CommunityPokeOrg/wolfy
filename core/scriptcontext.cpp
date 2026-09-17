#include "scriptcontext.h"
#include "scriptengine.h"

#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QProcess>
#include <QTimer>

ScriptContext::ScriptContext(const QString &path, ScriptEngine *engine)
    : QObject(engine)
    , m_path(path)
    , m_name(QFileInfo(path).fileName())
    , m_js(new QJSEngine(this))
    , m_engine(engine)
{
}

ScriptContext::~ScriptContext() = default;

// QML object literals arrive as QJSValues owned by the shell's engine and
// cannot be re-registered in a script's engine. toVariant() detaches them
// into plain QVariant data we can convert locally.
static QVariant detachPayload(const QVariant &payload)
{
    if (payload.metaType().id() == qMetaTypeId<QJSValue>())
        return payload.value<QJSValue>().toVariant();
    return payload;
}

bool ScriptContext::evaluate()
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        reportError(QStringLiteral("cannot open: %1").arg(file.errorString()), 0);
        return false;
    }
    const QString source = QString::fromUtf8(file.readAll());

    // Install the API before evaluating so top-level wolfy.* calls work.
    m_js->globalObject().setProperty(QStringLiteral("wolfy"), buildApiObject());

    QJSValue result = m_js->evaluate(source, m_path);
    if (result.isError()) {
        reportError(result.property(QStringLiteral("message")).toString(),
                    result.property(QStringLiteral("lineNumber")).toInt());
        return false;
    }

    m_loaded = true;
    return true;
}

void ScriptContext::dispatch(const QString &event, const QVariant &payload)
{
    const auto handlers = m_handlers.value(event);
    for (const QJSValue &handler : handlers) {
        if (!handler.isCallable())
            continue;
        // Convert into this script's own engine so handlers receive
        // native JS values, not a foreign QJSValue wrapper.
        QJSValue local = m_js->toScriptValue(detachPayload(payload));
        QJSValue result = handler.call({local});
        if (result.isError()) {
            reportError(QStringLiteral("%1 handler: %2")
                            .arg(event, result.property(QStringLiteral("message")).toString()),
                        result.property(QStringLiteral("lineNumber")).toInt());
        }
    }
}

QJSValue ScriptContext::buildApiObject()
{
    // __wolfy_ctx is the callable bridge; the wolfy.* functions below
    // wrap it so QJSValue arguments (callbacks) stay in this engine.
    m_js->globalObject().setProperty(QStringLiteral("__wolfy_ctx"),
                                     m_js->newQObject(this));

    QJSValue api = m_js->evaluate(QString::fromUtf8(R"JS(
(function() {
    var ctx = __wolfy_ctx;
    var api = {};
    api.on          = function(name, fn) { ctx._on(name, fn); };
    api.off         = function(name, fn) { ctx._off(name, fn); };
    api.emit        = function(name, payload) { ctx._emit(name, payload); };
    api.log         = function() { ctx._log("log",   Array.prototype.join.call(arguments, " ")); };
    api.warn        = function() { ctx._log("warn",  Array.prototype.join.call(arguments, " ")); };
    api.error       = function() { ctx._log("error", Array.prototype.join.call(arguments, " ")); };
    api.setTimeout  = function(fn, ms) { return ctx._setTimeout(fn, ms); };
    api.setInterval = function(fn, ms) { return ctx._setInterval(fn, ms); };
    api.clearTimer  = function(id)     { ctx._clearTimer(id); };
    api.exec        = function(cmd, cb) { ctx._exec(cmd, cb); };
    api.env         = function(name)    { return ctx._env(name); };
    api.quit        = function()        { ctx._quit(); };
    return api;
})()
    )JS"));

    api.setProperty(QStringLiteral("version"), QStringLiteral(WOLFY_VERSION));
    api.setProperty(QStringLiteral("scriptName"), m_name);
    api.setProperty(QStringLiteral("scriptPath"), m_path);
    if (m_engine->configObject().isValid())
        api.setProperty(QStringLiteral("config"),
                        m_js->toScriptValue(detachPayload(m_engine->configObject())));
    return api;
}

void ScriptContext::_on(const QString &name, const QJSValue &fn)
{
    if (fn.isCallable())
        m_handlers[name].append(fn);
}

void ScriptContext::_off(const QString &name, const QJSValue &fn)
{
    // QJSValue has no identity comparison; dropping the whole list is the
    // only reliable removal. Documented limitation: off() clears all
    // handlers for the event when called with any callable.
    if (fn.isCallable())
        m_handlers[name].clear();
}

void ScriptContext::_emit(const QString &name, const QJSValue &payload)
{
    emit emitted(name, payload.toVariant());
}

void ScriptContext::_log(const QString &level, const QString &message)
{
    emit m_engine->logMessage(level, message, m_name);
}

int ScriptContext::startTimer(const QJSValue &fn, int intervalMs, bool repeat)
{
    if (!fn.isCallable() || intervalMs < 0)
        return -1;
    auto *timer = new QTimer(this);
    timer->setSingleShot(!repeat);
    const int id = m_nextTimerId++;
    m_timers.insert(id, timer);
    connect(timer, &QTimer::timeout, this, [this, id, fn, repeat]() {
        QJSValue result = fn.call();
        if (result.isError()) {
            reportError(QStringLiteral("timer: %1")
                            .arg(result.property(QStringLiteral("message")).toString()),
                        result.property(QStringLiteral("lineNumber")).toInt());
        }
        if (!repeat)
            m_timers.remove(id); // QTimer itself dies with the context
    });
    timer->start(intervalMs);
    return id;
}

int ScriptContext::_setTimeout(const QJSValue &fn, int intervalMs)
{
    return startTimer(fn, intervalMs, false);
}

int ScriptContext::_setInterval(const QJSValue &fn, int intervalMs)
{
    return startTimer(fn, intervalMs, true);
}

void ScriptContext::_clearTimer(int id)
{
    QTimer *timer = m_timers.take(id);
    if (timer)
        timer->deleteLater();
}

void ScriptContext::_exec(const QString &command, const QJSValue &callback)
{
    if (!m_engine->allowExec()) {
        reportError(QStringLiteral("wolfy.exec disabled (allowExec=false)"), 0);
        return;
    }
    auto *proc = new QProcess(this);
    proc->setProgram(QStringLiteral("/bin/sh"));
    proc->setArguments({QStringLiteral("-c"), command});
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, callback](int code, QProcess::ExitStatus) {
                QString out = QString::fromLocal8Bit(proc->readAllStandardOutput());
                QString err = QString::fromLocal8Bit(proc->readAllStandardError());
                proc->deleteLater();
                if (!callback.isCallable())
                    return;
                QJSValue result = callback.call(
                    {m_js->toScriptValue(out.trimmed()),
                     m_js->toScriptValue(code),
                     m_js->toScriptValue(err.trimmed())});
                if (result.isError())
                    reportError(QStringLiteral("exec callback: %1")
                                    .arg(result.property(QStringLiteral("message")).toString()),
                                result.property(QStringLiteral("lineNumber")).toInt());
            });
    proc->start();
}

QString ScriptContext::_env(const QString &name) const
{
    return QString::fromLocal8Bit(qgetenv(name.toLocal8Bit().constData()));
}

void ScriptContext::_quit()
{
    m_js->throwError(QStringLiteral("wolfy.quit() called by %1").arg(m_name));
}

void ScriptContext::reportError(const QString &message, int line)
{
    m_error = message;
    emit m_engine->scriptError(m_path, message, line);
}
