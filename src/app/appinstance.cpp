#include "appinstance.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>

namespace LoofiPulse {

AppInstance::AppInstance(QObject *parent) : QObject(parent) {}

AppInstance::Claim AppInstance::claim(const QStringList &arguments, const QString &workingDirectory)
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface()) return Claim::Standalone;
    const QString name = QString::fromLatin1(ServiceName);
    const auto reply = bus.interface()->registerService(name, QDBusConnectionInterface::DontQueueService,
                                                        QDBusConnectionInterface::DontAllowReplacement);
    if (reply.isValid() && reply.value() == QDBusConnectionInterface::ServiceRegistered) {
        if (bus.registerObject(QString::fromLatin1(ObjectPath), this, QDBusConnection::ExportScriptableSlots))
            return Claim::Primary;
        bus.interface()->unregisterService(name);
        return Claim::Standalone;
    }
    auto call = QDBusMessage::createMethodCall(name, QString::fromLatin1(ObjectPath),
                                               QStringLiteral("com.github.loofi.hardwarepulse.App1"), QStringLiteral("Activate"));
    call << arguments << workingDirectory << QString::fromLocal8Bit(qgetenv("XDG_ACTIVATION_TOKEN"));
    const auto answer = bus.call(call, QDBus::Block, 5000);
    // An unresponsive owner must not prevent the user from opening a window.
    return answer.type() == QDBusMessage::ReplyMessage ? Claim::Forwarded : Claim::Standalone;
}

void AppInstance::Activate(const QStringList &arguments, const QString &workingDirectory, const QString &activationToken)
{
    Q_EMIT activateRequested(arguments, workingDirectory, activationToken);
}

}
