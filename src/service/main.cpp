#include "collectorservice.h"
#include "collectorprotocol.h"
#include <QGuiApplication>
#include <QDBusConnection>
#include <QSettings>
int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("loofi-pulse-service"));
    application.setDesktopFileName(QStringLiteral("com.github.loofi.hardwarepulse"));
    application.setQuitOnLastWindowClosed(false);
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) return 1;
    if (!bus.registerService(QString::fromLatin1(LoofiPulse::CollectorServiceName))) return 0;
    LoofiPulse::CollectorService service;
    if (!bus.registerObject(QString::fromLatin1(LoofiPulse::CollectorObjectPath), &service,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) return 2;
    return application.exec();
}
