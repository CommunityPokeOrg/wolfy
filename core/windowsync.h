#ifndef WOLFY_WINDOWSYNC_H
#define WOLFY_WINDOWSYNC_H

#include <QHash>
#include <QObject>
#include <QSet>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// WindowSync is Wolfy's cross-compositor window-state registry.
//
// Any number of window "sources" (a KWin scripting bridge, the local
// compositor's toplevel list, another shell instance, or scripts
// themselves) upsert window entries over the session D-Bus service
// org.wolfy.WindowSync or directly from QML. Each entry is keyed
// "<source>|<id>" and carries app identity, geometry, focus, workspace,
// and state flags.
//
// The point of the registry is *adoption*: when a window appears on a
// view, adopt(identity) claims the most recent known state for that app
// — from another source or from the persisted tombstone of a window that
// just went away — so shells don't re-run per-window setup for a window
// that merely moved between displays/views. A claimed identity stays
// adopted for adoptTtlMs so the same window can't be adopted twice.
//
// Loop prevention: suppress(identity) silences registry change signals
// for state we applied ourselves; suppression and adoption both expire.
//
// Entries persist to persistPath (JSON, 0600) so state survives shell
// restarts; stale entries are swept after staleMs.
class WindowSync : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_CLASSINFO("D-Bus Interface", "org.wolfy.WindowSync")
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString persistPath READ persistPath WRITE setPersistPath NOTIFY persistPathChanged)
    Q_PROPERTY(QVariantList windows READ windows NOTIFY windowsChanged)
    Q_PROPERTY(int windowCount READ windowCount NOTIFY windowsChanged)
    Q_PROPERTY(bool bridgeConnected READ bridgeConnected NOTIFY bridgeConnectedChanged)
    Q_PROPERTY(int adoptTtlMs MEMBER m_adoptTtlMs NOTIFY configChanged)
    Q_PROPERTY(int suppressMs MEMBER m_suppressMs NOTIFY configChanged)
    Q_PROPERTY(int staleMs MEMBER m_staleMs NOTIFY configChanged)

public:
    explicit WindowSync(QObject *parent = nullptr);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);
    QString persistPath() const { return m_persistPath; }
    void setPersistPath(const QString &path);
    QVariantList windows() const;
    int windowCount() const { return m_entries.size(); }
    bool bridgeConnected() const { return m_bridgeConnected; }

    // Normalize an app id (WM_CLASS / app_id / desktop file name) into a
    // comparison key: lower-cased, ".desktop" suffix stripped.
    static QString identityOf(const QString &appId);

public slots:
    // Merge entry into the registry. Required keys: source, id.
    // Emits windowAdded/windowUpdated unless the identity is suppressed.
    QVariantMap upsert(const QVariantMap &entry);
    // Tombstone key ("<source>|<id>") — kept for adoptGraceMs so a window
    // reappearing on another view can still adopt the state.
    QVariantMap remove(const QString &key);

    // Claim the newest known state for identity, excluding excludeKey
    // (typically the entry just upserted by the calling view). Returns {}
    // when there is nothing to adopt or the identity was already adopted
    // within adoptTtlMs — that second call is what prevents re-running
    // setup for the same window.
    QVariantMap adopt(const QString &identity, const QString &excludeKey = {});
    QVariantMap find(const QString &identity) const;

    // Silence signals for identity for suppressMs (echo suppression for
    // state we applied ourselves via the compositor).
    void suppress(const QString &identity, int ms = -1);
    bool isSuppressed(const QString &identity) const;

    // Source handshake. hello returns a snapshot of known windows so a
    // reconnecting source can reconcile; bye marks it offline. Sources
    // that go quiet past staleMs are also marked offline.
    QVariantMap hello(const QString &source);
    void bye(const QString &source);
    QVariantMap snapshot() const;
    bool ping() { return true; }

    // Re-announce the service on the session bus (recovery).
    bool registerService();

signals:
    void windowAdded(const QVariantMap &entry);
    void windowUpdated(const QVariantMap &entry);
    void windowRemoved(const QVariantMap &entry);
    void windowAdopted(const QVariantMap &entry);
    void bridgeConnectedChanged();
    void windowsChanged();
    void enabledChanged();
    void persistPathChanged();
    void configChanged();
    void logMessage(const QString &level, const QString &message);

public slots:
    void saveNow();
    void sweep();

private:
    QVariantMap mergeEntry(const QVariantMap &old, const QVariantMap &diff) const;
    bool suppressed(const QString &identity) const;
    void queueSave();
    void load();

    QHash<QString, QVariantMap> m_entries;
    QHash<QString, qint64> m_adoptedUntil;    // identity -> msecs
    QHash<QString, qint64> m_suppressedUntil; // identity -> msecs
    QHash<QString, qint64> m_sourceSeen;      // source   -> last activity msecs
    QSet<QString> m_onlineSources;            // sources that said hello
    QString m_persistPath;
    bool m_enabled = true;
    bool m_bridgeConnected = false;
    bool m_busTried = false;
    bool m_busOk = false;
    int m_adoptTtlMs = 60000;
    int m_suppressMs = 4000;
    int m_staleMs = 600000;
    int m_adoptGraceMs = 300000;
    QTimer m_saveTimer;
    QTimer m_sweepTimer;
};

#endif // WOLFY_WINDOWSYNC_H
