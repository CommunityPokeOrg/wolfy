#ifndef WOLFY_SCRIPTCONTEXT_H
#define WOLFY_SCRIPTCONTEXT_H

#include <QHash>
#include <QJSValue>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class QJSEngine;
class QTimer;
class ScriptEngine;

// One ScriptContext == one loaded .js file. Owns the QJSEngine, the
// registered event handlers and all timers spawned by the script so a
// reload can tear the whole thing down cleanly.
//
// The _on/_off/_emit/... methods are exposed to the script engine
// through a hidden __wolfy_ctx QObject and are wrapped by the wolfy.*
// functions installed on the global object — scripts never see them.
class ScriptContext : public QObject
{
    Q_OBJECT

public:
    ScriptContext(const QString &path, ScriptEngine *engine);
    ~ScriptContext() override;

    QString path() const { return m_path; }
    QString name() const { return m_name; }
    bool loaded() const { return m_loaded; }
    QString error() const { return m_error; }

    // Evaluate the file contents. Returns true on success; on failure
    // sets m_error and emits scriptError on the parent engine.
    bool evaluate();

    // Invoke all handlers registered for `event`.
    void dispatch(const QString &event, const QVariant &payload);

    // wolfy.* backing implementation (callable from this context's JS).
    Q_INVOKABLE void _on(const QString &name, const QJSValue &fn);
    Q_INVOKABLE void _off(const QString &name, const QJSValue &fn);
    Q_INVOKABLE void _emit(const QString &name, const QJSValue &payload);
    Q_INVOKABLE void _log(const QString &level, const QString &message);
    Q_INVOKABLE int _setTimeout(const QJSValue &fn, int intervalMs);
    Q_INVOKABLE int _setInterval(const QJSValue &fn, int intervalMs);
    Q_INVOKABLE void _clearTimer(int id);
    Q_INVOKABLE void _exec(const QString &command, const QJSValue &callback);
    Q_INVOKABLE QString _env(const QString &name) const;
    Q_INVOKABLE void _quit();
    Q_INVOKABLE QVariantList _syncWindows() const;
    Q_INVOKABLE QVariantMap _syncFind(const QString &identity) const;
    Q_INVOKABLE QVariantMap _syncAdopt(const QString &identity) const;
    Q_INVOKABLE QVariantMap _syncUpsert(const QVariantMap &entry);
    Q_INVOKABLE void _syncRemove(const QString &key);
    Q_INVOKABLE void _syncSuppress(const QString &identity, int ms);

signals:
    // Forwarded to ScriptEngine::eventFromScript.
    void emitted(const QString &name, const QVariant &payload);

private:
    QJSValue buildApiObject();
    int startTimer(const QJSValue &fn, int intervalMs, bool repeat);
    void reportError(const QString &message, int line);

    QString m_path;
    QString m_name;
    QJSEngine *m_js;
    bool m_loaded = false;
    QString m_error;
    QHash<QString, QList<QJSValue>> m_handlers;
    QHash<int, QTimer *> m_timers;
    int m_nextTimerId = 1;
    ScriptEngine *m_engine;
};

#endif // WOLFY_SCRIPTCONTEXT_H
