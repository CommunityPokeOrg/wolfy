#include "scriptengine.h"
#include "scriptcontext.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJSEngine>

ScriptEngine::ScriptEngine(QObject *parent)
    : QObject(parent)
    , m_watcher(new QFileSystemWatcher(this))
{
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &ScriptEngine::onFileChanged);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &ScriptEngine::onDirChanged);
}

ScriptEngine::~ScriptEngine() { unloadAll(); }

void ScriptEngine::setScriptDirs(const QStringList &dirs)
{
    if (m_scriptDirs == dirs)
        return;
    m_scriptDirs = dirs;
    emit scriptDirsChanged();
    rebuildWatchers();
    loadScripts();
}

void ScriptEngine::setAutoReload(bool on)
{
    if (m_autoReload == on)
        return;
    m_autoReload = on;
    emit autoReloadChanged();
    rebuildWatchers();
}

void ScriptEngine::setAllowExec(bool on)
{
    if (m_allowExec == on)
        return;
    m_allowExec = on;
    emit allowExecChanged();
}

void ScriptEngine::setConfigObject(const QVariant &config)
{
    m_config = config;
}

QVariantList ScriptEngine::scripts() const
{
    QVariantList out;
    for (const ScriptContext *ctx : m_contexts) {
        QVariantMap info;
        info[QStringLiteral("name")] = ctx->name();
        info[QStringLiteral("path")] = ctx->path();
        info[QStringLiteral("loaded")] = ctx->loaded();
        info[QStringLiteral("error")] = ctx->error();
        out.append(info);
    }
    return out;
}

void ScriptEngine::loadScripts()
{
    unloadAll();
    for (const QString &dirPath : m_scriptDirs) {
        QDir dir(dirPath);
        const QStringList files = dir.entryList({QStringLiteral("*.js")},
                                                QDir::Files, QDir::Name);
        for (const QString &file : files)
            loadScript(dir.absoluteFilePath(file));
    }
    emit scriptsChanged();
}

void ScriptEngine::unloadAll()
{
    const auto contexts = m_contexts;
    m_contexts.clear();
    m_byPath.clear();
    for (ScriptContext *ctx : contexts)
        ctx->deleteLater();
    emit scriptsChanged();
}

ScriptContext *ScriptEngine::loadScript(const QString &path)
{
    // Replace an existing context for the same path (reload case).
    if (ScriptContext *old = m_byPath.take(path)) {
        m_contexts.removeAll(old);
        old->deleteLater();
    }

    auto *ctx = new ScriptContext(path, this);
    connect(ctx, &ScriptContext::emitted,
            this, &ScriptEngine::eventFromScript);
    m_contexts.append(ctx);
    m_byPath.insert(path, ctx);

    if (ctx->evaluate())
        emit scriptLoaded(path);

    if (m_autoReload && !m_watcher->files().contains(path))
        m_watcher->addPath(path);

    emit scriptsChanged();
    return ctx;
}

void ScriptEngine::emitEvent(const QString &name, const QVariant &payload)
{
    for (ScriptContext *ctx : m_contexts) {
        if (ctx->loaded())
            ctx->dispatch(name, payload);
    }
}

void ScriptEngine::onFileChanged(const QString &path)
{
    if (!m_autoReload)
        return;
    // Editors often replace files, which drops the watch; re-add below.
    if (m_byPath.contains(path)) {
        emit logMessage(QStringLiteral("log"),
                        QStringLiteral("Reloading %1").arg(path),
                        QStringLiteral("ScriptEngine"));
        loadScript(path);
    } else if (QFileInfo(path).suffix() == QStringLiteral("js")) {
        loadScript(path);
    }
    // Re-add path: atomic saves remove the watch entry.
    if (QFileInfo::exists(path) && !m_watcher->files().contains(path))
        m_watcher->addPath(path);
}

void ScriptEngine::onDirChanged(const QString &path)
{
    if (!m_autoReload)
        return;
    QDir dir(path);
    const auto known = m_byPath.keys();
    // Load new files that appeared in the directory.
    const QStringList files = dir.entryList({QStringLiteral("*.js")},
                                            QDir::Files, QDir::Name);
    for (const QString &file : files) {
        const QString abs = dir.absoluteFilePath(file);
        if (!m_byPath.contains(abs))
            loadScript(abs);
    }
    if (QFileInfo::exists(path) && !m_watcher->directories().contains(path))
        m_watcher->addPath(path);
}

void ScriptEngine::rebuildWatchers()
{
    if (!m_watcher->files().isEmpty())
        m_watcher->removePaths(m_watcher->files());
    if (!m_watcher->directories().isEmpty())
        m_watcher->removePaths(m_watcher->directories());
    if (!m_autoReload)
        return;
    for (const QString &dir : m_scriptDirs) {
        if (QFileInfo::exists(dir))
            m_watcher->addPath(dir);
    }
    for (const QString &file : m_byPath.keys())
        m_watcher->addPath(file);
}
