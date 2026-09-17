#include "windowsync.h"

#include <QDBusConnection>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

static const QLatin1String DBUS_SERVICE("org.wolfy.WindowSync");
static const QLatin1String DBUS_PATH("/sync");

WindowSync::WindowSync(QObject *parent)
    : QObject(parent)
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(500);
    connect(&m_saveTimer, &QTimer::timeout, this, &WindowSync::saveNow);

    m_sweepTimer.setInterval(30000);
    connect(&m_sweepTimer, &QTimer::timeout, this, &WindowSync::sweep);
    m_sweepTimer.start();

    registerService();
}

QString WindowSync::identityOf(const QString &appId)
{
    QString s = appId.trimmed().toLower();
    if (s.endsWith(QStringLiteral(".desktop")))
        s.chop(8);
    return s;
}

void WindowSync::setEnabled(bool on)
{
    if (m_enabled == on)
        return;
    m_enabled = on;
    if (on)
        registerService();
    emit enabledChanged();
}

void WindowSync::setPersistPath(const QString &path)
{
    if (m_persistPath == path)
        return;
    m_persistPath = path;
    load();
    emit persistPathChanged();
}

QVariantList WindowSync::windows() const
{
    QVariantList out;
    out.reserve(m_entries.size());
    for (const QVariantMap &e : m_entries)
        out.append(e);
    return out;
}

QVariantMap WindowSync::mergeEntry(const QVariantMap &old, const QVariantMap &diff) const
{
    QVariantMap merged = old;
    for (auto it = diff.constBegin(); it != diff.constEnd(); ++it)
        merged.insert(it.key(), it.value());
    return merged;
}

QVariant WindowSync::sanitizeValue(const QVariant &v) const
{
    const int t = v.userType();
    // Values libdbus cannot marshal (Nullptr from JS null/undefined,
    // invalid or custom types) must never reach signals or storage —
    // dbus-daemon abort()s the sender on marshall failure.
    if (!v.isValid() || t == QMetaType::Nullptr || t == QMetaType::UnknownType)
        return {};
    if (t == QMetaType::QVariantMap)
        return sanitize(v.toMap());
    if (t == QMetaType::QVariantList) {
        QVariantList out;
        for (const QVariant &item : v.toList()) {
            QVariant s = sanitizeValue(item);
            if (s.isValid())
                out.append(s);
        }
        return out;
    }
    return v;
}

QVariantMap WindowSync::sanitize(const QVariantMap &in) const
{
    QVariantMap out;
    for (auto it = in.constBegin(); it != in.constEnd(); ++it) {
        QVariant s = sanitizeValue(it.value());
        if (s.isValid())
            out.insert(it.key(), s);
    }
    return out;
}

bool WindowSync::suppressed(const QString &identity) const
{
    return m_suppressedUntil.value(identity) > QDateTime::currentMSecsSinceEpoch();
}

QVariantMap WindowSync::upsert(const QVariantMap &entry)
{
    if (!m_enabled)
        return {};
    const QString source = entry.value(QStringLiteral("source")).toString();
    const QString id = entry.value(QStringLiteral("id")).toString();
    if (source.isEmpty() || id.isEmpty())
        return {};

    const QString key = source + QLatin1Char('|') + id;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool isNew = !m_entries.contains(key);

    const QVariantMap clean = sanitize(entry);
    QVariantMap merged = isNew ? clean : mergeEntry(m_entries.value(key), clean);
    merged[QStringLiteral("key")] = key;
    merged[QStringLiteral("source")] = source;
    merged[QStringLiteral("id")] = id;
    merged[QStringLiteral("identity")] =
        identityOf(merged.value(QStringLiteral("appId")).toString());
    merged[QStringLiteral("lastSeen")] = now;
    merged[QStringLiteral("gone")] = false;
    if (isNew)
        merged[QStringLiteral("adopted")] = false;

    m_entries.insert(key, merged);
    m_sourceSeen[source] = now;
    queueSave();

    const QString identity = merged.value(QStringLiteral("identity")).toString();
    if (!identity.isEmpty() && !suppressed(identity)) {
        if (isNew)
            emit windowAdded(merged);
        else
            emit windowUpdated(merged);
    }
    emit windowsChanged();
    return merged;
}

QVariantMap WindowSync::remove(const QString &key)
{
    if (!m_entries.contains(key))
        return {};
    QVariantMap e = m_entries.value(key);
    // Sources may fire remove twice for one window (KWin emits both a
    // per-window closed signal and clientRemoved) — only the first is
    // an event.
    const bool alreadyGone = e.value(QStringLiteral("gone")).toBool();
    e[QStringLiteral("gone")] = true;
    e[QStringLiteral("lastSeen")] = QDateTime::currentMSecsSinceEpoch();
    m_entries.insert(key, e);
    queueSave();

    const QString identity = e.value(QStringLiteral("identity")).toString();
    if (!alreadyGone && !identity.isEmpty() && !suppressed(identity))
        emit windowRemoved(e);
    emit windowsChanged();
    return e;
}

QVariantMap WindowSync::adopt(const QString &identity, const QString &excludeKey)
{
    const QString ident = identityOf(identity);
    if (ident.isEmpty())
        return {};
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    // Identity already claimed recently: adopting again would re-run setup
    // for the same logical window — that's the loop/re-run prevention.
    if (m_adoptedUntil.value(ident) > now)
        return {};

    QVariantMap best;
    qint64 bestSeen = -1;
    for (const QVariantMap &e : m_entries) {
        if (e.value(QStringLiteral("key")).toString() == excludeKey)
            continue;
        if (e.value(QStringLiteral("identity")).toString() != ident)
            continue;
        const bool gone = e.value(QStringLiteral("gone")).toBool();
        if (gone && e.value(QStringLiteral("lastSeen")).toLongLong() < now - m_adoptGraceMs)
            continue; // tombstone too old to trust
        const qint64 seen = e.value(QStringLiteral("lastSeen")).toLongLong();
        if (seen > bestSeen) {
            bestSeen = seen;
            best = e;
        }
    }
    if (best.isEmpty())
        return {};

    m_adoptedUntil.insert(ident, now + m_adoptTtlMs);
    best[QStringLiteral("adopted")] = true;
    m_entries.insert(best.value(QStringLiteral("key")).toString(), best);
    queueSave();
    emit windowAdopted(best);
    return best;
}

QVariantMap WindowSync::find(const QString &identity) const
{
    const QString ident = identityOf(identity);
    QVariantMap best;
    qint64 bestSeen = -1;
    for (const QVariantMap &e : m_entries) {
        if (e.value(QStringLiteral("identity")).toString() != ident)
            continue;
        if (e.value(QStringLiteral("gone")).toBool())
            continue;
        const qint64 seen = e.value(QStringLiteral("lastSeen")).toLongLong();
        if (seen > bestSeen) {
            bestSeen = seen;
            best = e;
        }
    }
    return best;
}

void WindowSync::suppress(const QString &identity, int ms)
{
    const QString ident = identityOf(identity);
    if (ident.isEmpty())
        return;
    m_suppressedUntil.insert(ident, QDateTime::currentMSecsSinceEpoch()
                                        + (ms > 0 ? ms : m_suppressMs));
}

bool WindowSync::isSuppressed(const QString &identity) const
{
    return suppressed(identityOf(identity));
}

QVariantMap WindowSync::hello(const QString &source)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_sourceSeen[source] = now;
    m_onlineSources.insert(source);
    if (!m_bridgeConnected) {
        m_bridgeConnected = true;
        emit bridgeConnectedChanged();
    }
    return snapshot();
}

void WindowSync::bye(const QString &source)
{
    m_onlineSources.remove(source);
    m_sourceSeen.remove(source);
    if (m_bridgeConnected && m_onlineSources.isEmpty()) {
        m_bridgeConnected = false;
        emit bridgeConnectedChanged();
    }
}

QVariantMap WindowSync::snapshot() const
{
    QVariantMap snap;
    snap[QStringLiteral("windows")] = windows();
    snap[QStringLiteral("now")] = QDateTime::currentMSecsSinceEpoch();
    QVariantList srcs;
    for (auto it = m_sourceSeen.constBegin(); it != m_sourceSeen.constEnd(); ++it)
        srcs.append(it.key());
    snap[QStringLiteral("sources")] = srcs;
    return snap;
}

bool WindowSync::registerService()
{
    if (m_busTried)
        return m_busOk;
    m_busTried = true;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        emit logMessage(QStringLiteral("warn"),
                        QStringLiteral("WindowSync: no session bus; registry is local-only"));
        return false;
    }
    if (!bus.registerService(DBUS_SERVICE)) {
        // Another instance already owns the name (second shell, stale
        // process). We still export the object path on our connection;
        // callers talking to the service name reach the owner.
        emit logMessage(QStringLiteral("warn"),
                        QStringLiteral("WindowSync: %1 already registered")
                            .arg(DBUS_SERVICE));
    }
    m_busOk = bus.registerObject(DBUS_PATH, this,
                                 QDBusConnection::ExportAllSlots
                                     | QDBusConnection::ExportAllInvokables
                                     | QDBusConnection::ExportAllSignals);
    emit logMessage(QStringLiteral("log"),
                    QStringLiteral("WindowSync: listening on %1 %2")
                        .arg(DBUS_SERVICE, DBUS_PATH));
    return m_busOk;
}

void WindowSync::queueSave()
{
    if (!m_persistPath.isEmpty() && !m_saveTimer.isActive())
        m_saveTimer.start();
}

void WindowSync::saveNow()
{
    if (m_persistPath.isEmpty())
        return;
    QSaveFile f(m_persistPath);
    if (!f.open(QIODevice::WriteOnly)) {
        emit logMessage(QStringLiteral("warn"),
                        QStringLiteral("WindowSync: cannot write %1").arg(m_persistPath));
        return;
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("windows"), QJsonArray::fromVariantList(windows()));
    f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    if (!f.commit())
        return;
    QFile::setPermissions(m_persistPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

void WindowSync::load()
{
    QFile f(m_persistPath);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    const QVariantList list =
        doc.object().value(QStringLiteral("windows")).toArray().toVariantList();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const QVariant &v : list) {
        const QVariantMap e = v.toMap();
        const QString key = e.value(QStringLiteral("key")).toString();
        if (key.isEmpty() || m_entries.contains(key))
            continue;
        // Restored entries keep their own lastSeen; the sweeper drops
        // anything older than staleMs so stale state can't linger.
        // JSON nulls restore as invalid QVariants — sanitize so they
        // can't poison a signal's QVariantMap payload later.
        QVariantMap entry = sanitize(e);
        entry[QStringLiteral("restored")] = true;
        entry[QStringLiteral("adopted")] = false;
        entry[QStringLiteral("gone")] = true; // unconfirmed until re-seen
        m_entries.insert(key, entry);
    }
    Q_UNUSED(now);
}

void WindowSync::sweep()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool dirty = false;
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        const qint64 seen = it->value(QStringLiteral("lastSeen")).toLongLong();
        if (seen < now - m_staleMs) {
            it = m_entries.erase(it);
            dirty = true;
        } else {
            ++it;
        }
    }
    for (auto it = m_sourceSeen.begin(); it != m_sourceSeen.end();) {
        if (it.value() < now - m_staleMs) {
            m_onlineSources.remove(it.key());
            it = m_sourceSeen.erase(it);
        } else {
            ++it;
        }
    }
    if (m_bridgeConnected && m_onlineSources.isEmpty()) {
        m_bridgeConnected = false;
        emit bridgeConnectedChanged();
    }
    if (dirty) {
        queueSave();
        emit windowsChanged();
    }
}
