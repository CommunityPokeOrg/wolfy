#ifndef WOLFY_SCRIPTENGINE_H
#define WOLFY_SCRIPTENGINE_H

#include <QHash>
#include <QJSValue>
#include <QObject>
#include <QVariant>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QFileSystemWatcher;
class QTimer;
class ScriptContext;

// ScriptEngine is the heart of Wolfy's JavaScript scripting support.
// It loads every *.js file found in scriptDirs into its own QJSEngine,
// injects a "wolfy" API object, and bridges events between QML and JS.
//
// QML -> JS: call emitEvent(name, payload). Every handler registered
//            via wolfy.on(name, fn) in every script is invoked.
// JS  -> QML: scripts call wolfy.emit(name, payload) which raises the
//            eventFromScript signal that QML can handle.
class ScriptEngine : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QStringList scriptDirs READ scriptDirs WRITE setScriptDirs NOTIFY scriptDirsChanged)
    Q_PROPERTY(bool autoReload READ autoReload WRITE setAutoReload NOTIFY autoReloadChanged)
    Q_PROPERTY(bool allowExec READ allowExec WRITE setAllowExec NOTIFY allowExecChanged)
    Q_PROPERTY(QVariantList scripts READ scripts NOTIFY scriptsChanged)
    Q_PROPERTY(int loadedCount READ loadedCount NOTIFY scriptsChanged)

public:
    explicit ScriptEngine(QObject *parent = nullptr);
    ~ScriptEngine() override;

    QStringList scriptDirs() const { return m_scriptDirs; }
    void setScriptDirs(const QStringList &dirs);

    bool autoReload() const { return m_autoReload; }
    void setAutoReload(bool on);

    bool allowExec() const { return m_allowExec; }
    void setAllowExec(bool on);

    QVariantList scripts() const;
    int loadedCount() const { return m_contexts.size(); }

    // Installed into each script engine as wolfy.config.
    QVariant configObject() const { return m_config; }
    Q_INVOKABLE void setConfigObject(const QVariant &config);

public slots:
    void loadScripts();
    void unloadAll();
    // Fire an event into every script's wolfy.on() handlers.
    void emitEvent(const QString &name, const QVariant &payload = QVariant());

signals:
    // Raised when a script calls wolfy.emit(name, payload).
    void eventFromScript(const QString &name, const QVariant &payload);
    // wolfy.log()/warn()/error() route here. level: "log" | "warn" | "error".
    void logMessage(const QString &level, const QString &message, const QString &source);
    void scriptLoaded(const QString &path);
    void scriptError(const QString &path, const QString &message, int line);
    void scriptDirsChanged();
    void autoReloadChanged();
    void allowExecChanged();
    void scriptsChanged();

private slots:
    void onFileChanged(const QString &path);
    void onDirChanged(const QString &path);

private:
    ScriptContext *loadScript(const QString &path);
    void rebuildWatchers();

    QStringList m_scriptDirs;
    bool m_autoReload = true;
    bool m_allowExec = true;
    QList<ScriptContext *> m_contexts;
    QHash<QString, ScriptContext *> m_byPath;
    QFileSystemWatcher *m_watcher;
    QVariant m_config;
};

#endif // WOLFY_SCRIPTENGINE_H
