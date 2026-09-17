#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>

#include "scriptengine.h"

// End-to-end tests for the Wolfy JS runtime: loading, events in both
// directions, timers, exec, hot reload and error reporting.
class TestScriptEngine : public QObject
{
    Q_OBJECT

    QString writeScript(QTemporaryDir &tmp, const QString &name,
                        const QString &source)
    {
        const QString path = tmp.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            return {};
        f.write(source.toUtf8());
        return path;
    }

private slots:
    void loadAndDispatch()
    {
        QTemporaryDir tmp;
        writeScript(tmp, QStringLiteral("echo.js"), QStringLiteral(
            "wolfy.on('ping', function(d){ wolfy.emit('pong', d + 1); });"));

        ScriptEngine engine;
        QSignalSpy emitted(&engine, &ScriptEngine::eventFromScript);
        engine.setScriptDirs({tmp.path()});

        QCOMPARE(engine.loadedCount(), 1);

        engine.emitEvent(QStringLiteral("ping"), 41);
        QCoreApplication::processEvents();

        QCOMPARE(emitted.count(), 1);
        QCOMPARE(emitted.at(0).at(0).toString(), QStringLiteral("pong"));
        QCOMPARE(emitted.at(0).at(1).toInt(), 42);
    }

    void syntaxErrorIsReported()
    {
        QTemporaryDir tmp;
        writeScript(tmp, QStringLiteral("bad.js"), QStringLiteral("this is not js !!!"));

        ScriptEngine engine;
        QSignalSpy errors(&engine, &ScriptEngine::scriptError);
        engine.setScriptDirs({tmp.path()});

        QCOMPARE(errors.count(), 1);
        QCOMPARE(engine.scripts().at(0).toMap()
                     .value(QStringLiteral("loaded")).toBool(), false);
    }

    void logAndExec()
    {
        QTemporaryDir tmp;
        writeScript(tmp, QStringLiteral("exec.js"), QStringLiteral(
            "wolfy.log('booting');"
            "wolfy.exec('echo -n hi', function(out, code){"
            "    wolfy.emit('execdone', out + ':' + code);"
            "});"));

        ScriptEngine engine;
        QSignalSpy logs(&engine, &ScriptEngine::logMessage);
        QSignalSpy emitted(&engine, &ScriptEngine::eventFromScript);
        engine.setScriptDirs({tmp.path()});

        QVERIFY(logs.count() >= 1);
        QCOMPARE(logs.at(0).at(1).toString(), QStringLiteral("booting"));

        QVERIFY(emitted.wait(3000));
        QCOMPARE(emitted.at(0).at(0).toString(), QStringLiteral("execdone"));
        QCOMPARE(emitted.at(0).at(1).toString(), QStringLiteral("hi:0"));
    }

    void execCanBeDisabled()
    {
        QTemporaryDir tmp;
        writeScript(tmp, QStringLiteral("exec.js"), QStringLiteral(
            "wolfy.exec('echo hi', function(){});"));

        ScriptEngine engine;
        engine.setAllowExec(false);
        QSignalSpy errors(&engine, &ScriptEngine::scriptError);
        engine.setScriptDirs({tmp.path()});
        QCoreApplication::processEvents();

        QVERIFY(errors.count() >= 1);
    }

    void timersFire()
    {
        QTemporaryDir tmp;
        writeScript(tmp, QStringLiteral("timer.js"), QStringLiteral(
            "var n = 0;"
            "wolfy.setTimeout(function(){ wolfy.emit('tick', ++n); }, 20);"
            "wolfy.setInterval(function(){ wolfy.emit('tock', ++n); }, 15);"));

        ScriptEngine engine;
        QSignalSpy emitted(&engine, &ScriptEngine::eventFromScript);
        engine.setScriptDirs({tmp.path()});

        QVERIFY(emitted.wait(2000));      // tick
        QVERIFY(emitted.wait(2000));      // tock
        const QStringList names = {emitted.at(0).at(0).toString(),
                                   emitted.at(1).at(0).toString()};
        QVERIFY(names.contains(QStringLiteral("tick")));
        QVERIFY(names.contains(QStringLiteral("tock")));
    }

    void hotReload()
    {
        QTemporaryDir tmp;
        const QString path = writeScript(tmp, QStringLiteral("live.js"), QStringLiteral(
            "wolfy.on('ping', function(){ wolfy.emit('v', 1); });"));

        ScriptEngine engine;
        QSignalSpy emitted(&engine, &ScriptEngine::eventFromScript);
        engine.setScriptDirs({tmp.path()});

        // Rewrite the script; the watcher should reload it.
        QSignalSpy loaded(&engine, &ScriptEngine::scriptLoaded);
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("wolfy.on('ping', function(){ wolfy.emit('v', 2); });");
        f.close();

        QVERIFY(loaded.wait(5000));
        QTest::qWait(100); // let any duplicate watcher events settle

        emitted.clear();
        engine.emitEvent(QStringLiteral("ping"));
        QCoreApplication::processEvents();
        QCOMPARE(emitted.count(), 1);
        QCOMPARE(emitted.at(0).at(1).toInt(), 2);
    }

    void foreignEnginePayloadIsDetached()
    {
        // QML object literals reach emitEvent() as QJSValues owned by the
        // shell engine; they must be detached before crossing into a
        // script's engine.
        QTemporaryDir tmp;
        writeScript(tmp, QStringLiteral("recv.js"), QStringLiteral(
            "wolfy.on('data', function(d){ wolfy.emit('seen', d.name); });"));

        QJSEngine foreign;
        const QVariant payload =
            QVariant::fromValue(foreign.evaluate("({name: 'w1'})"));

        ScriptEngine engine;
        QSignalSpy emitted(&engine, &ScriptEngine::eventFromScript);
        engine.setScriptDirs({tmp.path()});
        engine.emitEvent(QStringLiteral("data"), payload);
        QCoreApplication::processEvents();

        QCOMPARE(emitted.count(), 1);
        QCOMPARE(emitted.at(0).at(0).toString(), QStringLiteral("seen"));
        QCOMPARE(emitted.at(0).at(1).toString(), QStringLiteral("w1"));
    }

    void configReachesScripts()
    {
        QTemporaryDir tmp;
        writeScript(tmp, QStringLiteral("cfg.js"), QStringLiteral(
            "wolfy.on('go', function(){ wolfy.emit('cfg', wolfy.config.color); });"));

        ScriptEngine engine;
        engine.setConfigObject(QVariantMap{{QStringLiteral("color"),
                                            QStringLiteral("purple")}});

        QSignalSpy emitted(&engine, &ScriptEngine::eventFromScript);
        engine.setScriptDirs({tmp.path()});
        engine.emitEvent(QStringLiteral("go"));

        QCOMPARE(emitted.count(), 1);
        QCOMPARE(emitted.at(0).at(1).toString(), QStringLiteral("purple"));
    }
};

QTEST_GUILESS_MAIN(TestScriptEngine)
#include "test_scriptengine.moc"
