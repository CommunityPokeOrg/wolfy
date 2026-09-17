// wolfy-sync-relay — forwards org.wolfy.WindowSync /sync calls between two
// D-Bus session buses (e.g. KWin running on a dbus-launch bus while Wolfy
// runs on the systemd user bus). Method calls arriving on the "front" bus
// are replayed on the "back" bus and the reply is returned verbatim.
//
//   wolfy-sync-relay <front-bus-address>
//
// The back bus is the normal session bus (DBUS_SESSION_BUS_ADDRESS). The
// front bus is given explicitly — pass the value found in
// ~/.dbus/session-bus/<machine>-<display> when KWin lives on a
// dbus-launch session.

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QDebug>

namespace {
const auto kService = QStringLiteral("org.wolfy.WindowSync");
const auto kPath = QStringLiteral("/sync");

class Relay : public QDBusVirtualObject {
  public:
    explicit Relay(const QDBusConnection &back, QObject *parent = nullptr)
        : QDBusVirtualObject(parent), m_back(back) {}

    QString introspect(const QString &) const override { return QString(); }

    bool handleMessage(const QDBusMessage &msg,
                       const QDBusConnection &conn) override {
        if (msg.type() != QDBusMessage::MethodCallMessage)
            return false;
        QDBusMessage fwd = QDBusMessage::createMethodCall(
            kService, msg.path(), msg.interface(), msg.member());
        fwd.setArguments(msg.arguments());
        QDBusMessage reply = m_back.call(fwd);
        QDBusMessage out =
            reply.type() == QDBusMessage::ErrorMessage
                ? msg.createErrorReply(reply.errorName(),
                                       reply.errorMessage())
                : msg.createReply(reply.arguments());
        conn.send(out);
        return true;
    }

  private:
    QDBusConnection m_back;
};
} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        qWarning("usage: wolfy-sync-relay <front-bus-address>");
        return 2;
    }

    QDBusConnection back = QDBusConnection::sessionBus();
    if (!back.isConnected()) {
        qWarning("back bus (session) unreachable");
        return 1;
    }

    QDBusConnection front = QDBusConnection::connectToBus(
        QString::fromLocal8Bit(argv[1]), QStringLiteral("wolfy-front"));
    if (!front.isConnected()) {
        qWarning("front bus %s unreachable", argv[1]);
        return 1;
    }
    front.registerVirtualObject(
        kPath, new Relay(back, &app),
        QDBusConnection::SubPath);
    if (!front.registerService(kService)) {
        qWarning("cannot own %s on front bus: %s", qPrintable(kService),
                 qPrintable(front.lastError().message()));
        return 1;
    }
    qInfo("sync-relay: forwarding %s %s (front %s -> back session)",
          qPrintable(kService), qPrintable(kPath), argv[1]);
    return app.exec();
}
