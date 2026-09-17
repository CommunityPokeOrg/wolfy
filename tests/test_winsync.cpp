#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>

#include "windowsync.h"

// Registry behaviour: upsert/remove, adoption claims + TTL (re-run
// prevention), echo suppression, tombstone grace, persistence
// round-trip, source handshake and stale sweeping.
class TestWindowSync : public QObject
{
    Q_OBJECT

    static QVariantMap entry(const QString &source, const QString &id,
                             const QString &appId)
    {
        return {{QStringLiteral("source"), source},
                {QStringLiteral("id"), id},
                {QStringLiteral("appId"), appId}};
    }

private slots:
    void upsertAddsAndMerges()
    {
        WindowSync sync;
        QSignalSpy added(&sync, &WindowSync::windowAdded);
        QSignalSpy updated(&sync, &WindowSync::windowUpdated);

        QVariantMap e = sync.upsert(entry(QStringLiteral("kwin"),
                                          QStringLiteral("1"),
                                          QStringLiteral("Konsole")));
        QCOMPARE(e.value(QStringLiteral("key")).toString(),
                 QStringLiteral("kwin|1"));
        QCOMPARE(e.value(QStringLiteral("identity")).toString(),
                 QStringLiteral("konsole"));
        QCOMPARE(added.count(), 1);

        // Partial update merges, keeping appId.
        sync.upsert({{QStringLiteral("source"), QStringLiteral("kwin")},
                     {QStringLiteral("id"), QStringLiteral("1")},
                     {QStringLiteral("workspace"), QStringLiteral("2")}});
        QCOMPARE(updated.count(), 1);
        const QVariantMap got = sync.find(QStringLiteral("konsole"));
        QCOMPARE(got.value(QStringLiteral("appId")).toString(),
                 QStringLiteral("Konsole"));
        QCOMPARE(got.value(QStringLiteral("workspace")).toString(),
                 QStringLiteral("2"));
    }

    void adoptClaimsOnceWithinTtl()
    {
        WindowSync sync;
        sync.upsert(entry(QStringLiteral("kwin"), QStringLiteral("9"),
                          QStringLiteral("Firefox")));

        QVariantMap claimed = sync.adopt(QStringLiteral("firefox"),
                                         QStringLiteral("local|1"));
        QCOMPARE(claimed.value(QStringLiteral("id")).toString(),
                 QStringLiteral("9"));
        QCOMPARE(claimed.value(QStringLiteral("adopted")).toBool(), true);

        // Same identity inside the TTL: nothing to adopt → the caller
        // must not re-run setup for the same logical window.
        QVERIFY(sync.adopt(QStringLiteral("firefox"),
                           QStringLiteral("local|2")).isEmpty());
    }

    void adoptSkipsExcludedEntry()
    {
        WindowSync sync;
        sync.upsert(entry(QStringLiteral("local"), QStringLiteral("1"),
                          QStringLiteral("foot")));
        // Only the local entry exists; excluding it leaves no candidate.
        QVERIFY(sync.adopt(QStringLiteral("foot"),
                           QStringLiteral("local|1")).isEmpty());
    }

    void tombstoneIsAdoptableWithinGrace()
    {
        WindowSync sync;
        sync.upsert(entry(QStringLiteral("kwin"), QStringLiteral("7"),
                          QStringLiteral("Dolphin")));
        sync.remove(QStringLiteral("kwin|7"));

        const QVariantMap claimed = sync.adopt(QStringLiteral("dolphin"));
        QCOMPARE(claimed.value(QStringLiteral("key")).toString(),
                 QStringLiteral("kwin|7"));
    }

    void suppressionSilencesEvents()
    {
        WindowSync sync;
        sync.suppress(QStringLiteral("mpv"), 60000);
        QSignalSpy added(&sync, &WindowSync::windowAdded);
        QSignalSpy updated(&sync, &WindowSync::windowUpdated);

        sync.upsert(entry(QStringLiteral("local"), QStringLiteral("1"),
                          QStringLiteral("mpv")));
        sync.upsert(entry(QStringLiteral("local"), QStringLiteral("1"),
                          QStringLiteral("mpv")));
        QCOMPARE(added.count(), 0);
        QCOMPARE(updated.count(), 0);
        // Registry still tracks the window — suppression is only about
        // event noise from self-applied state.
        QCOMPARE(sync.windowCount(), 1);
    }

    void identityNormalizes()
    {
        QCOMPARE(WindowSync::identityOf(QStringLiteral("Org.Kde.Dolphin.desktop")),
                 QStringLiteral("org.kde.dolphin"));
        QCOMPARE(WindowSync::identityOf(QStringLiteral("Foot")),
                 QStringLiteral("foot"));
    }

    void persistenceRoundTrip()
    {
        QTemporaryDir tmp;
        const QString path = tmp.filePath(QStringLiteral("sync.json"));

        WindowSync first;
        first.setPersistPath(path);
        first.upsert(entry(QStringLiteral("kwin"), QStringLiteral("3"),
                           QStringLiteral("Kate")));
        QTest::qWait(700); // debounced save

        WindowSync second;
        second.setPersistPath(path);
        const QVariantMap e = second.snapshot()
                                  .value(QStringLiteral("windows")).toList()
                                  .at(0).toMap();
        QCOMPARE(e.value(QStringLiteral("key")).toString(),
                 QStringLiteral("kwin|3"));
        // Restored entries start unconfirmed (gone) — the live upsert on
        // reconnect flips them back.
        QCOMPARE(e.value(QStringLiteral("gone")).toBool(), true);
        QCOMPARE(QFile::permissions(path) & QFileDevice::ReadOther,
                 QFileDevice::Permissions{});
    }

    void helloSnapshotAndBye()
    {
        WindowSync sync;
        QSignalSpy conn(&sync, &WindowSync::bridgeConnectedChanged);
        QVERIFY(!sync.bridgeConnected());

        sync.upsert(entry(QStringLiteral("local"), QStringLiteral("1"),
                          QStringLiteral("foot")));
        const QVariantMap snap = sync.hello(QStringLiteral("kwin"));
        QVERIFY(sync.bridgeConnected());
        QCOMPARE(conn.count(), 1);
        QCOMPARE(snap.value(QStringLiteral("windows")).toList().size(), 1);

        sync.bye(QStringLiteral("kwin"));
        QVERIFY(!sync.bridgeConnected());
        QCOMPARE(conn.count(), 2);
    }

    void sweepExpiresStaleEntries()
    {
        WindowSync sync;
        sync.setProperty("staleMs", 50);
        sync.upsert(entry(QStringLiteral("kwin"), QStringLiteral("4"),
                          QStringLiteral("Old")));
        QCOMPARE(sync.windowCount(), 1);
        QTest::qWait(1200);
        sync.sweep(); // public slot — force it rather than waiting 30s
        QCOMPARE(sync.windowCount(), 0);
    }

    void upsertStripsUnmarshallableValues()
    {
        // JS sources can hand us null/undefined fields, which arrive
        // as QMetaType::Nullptr — a type libdbus cannot marshal, so
        // emitting it in a signal kills the process. They must be
        // dropped at the door.
        WindowSync sync;
        QSignalSpy added(&sync, &WindowSync::windowAdded);
        QVariantMap bad = entry(QStringLiteral("kwin"), QStringLiteral("5"),
                                QStringLiteral("Nulls"));
        bad.insert(QStringLiteral("rect"),
                   QVariant::fromValue(std::nullptr_t()));
        bad.insert(QStringLiteral("note"), QVariant());
        QVariantMap nested;
        nested.insert(QStringLiteral("x"), 1);
        nested.insert(QStringLiteral("bad"),
                      QVariant::fromValue(std::nullptr_t()));
        bad.insert(QStringLiteral("meta"), nested);

        const QVariantMap e = sync.upsert(bad);
        QVERIFY(!e.contains(QStringLiteral("rect")));
        QVERIFY(!e.contains(QStringLiteral("note")));
        const QVariantMap meta = e.value(QStringLiteral("meta")).toMap();
        QCOMPARE(meta.value(QStringLiteral("x")).toInt(), 1);
        QVERIFY(!meta.contains(QStringLiteral("bad")));
        QCOMPARE(added.count(), 1);
    }
};

QTEST_GUILESS_MAIN(TestWindowSync)
#include "test_winsync.moc"
