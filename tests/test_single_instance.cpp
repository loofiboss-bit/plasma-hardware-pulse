#include "appinstance.h"
#include <QCoreApplication>
#include <QProcess>
#include <QSignalSpy>
#include <QtTest>

using namespace LoofiPulse;

class TestSingleInstance : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testPrimaryAndForwardClaim()
    {
        AppInstance primary;
        const auto claim1 = primary.claim({QStringLiteral("--page"), QStringLiteral("power")}, QStringLiteral("/tmp"));
        QCOMPARE(claim1, AppInstance::Claim::Primary);

        QSignalSpy spy(&primary, &AppInstance::activateRequested);

        QProcess secondaryProc;
        secondaryProc.start(QCoreApplication::applicationFilePath(), {QStringLiteral("sublaunch")});
        QTRY_VERIFY_WITH_TIMEOUT(secondaryProc.state() == QProcess::NotRunning, 5000);
        QCOMPARE(secondaryProc.exitCode(), 0);

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
        const auto args = spy.first().at(0).toStringList();
        QCOMPARE(args, QStringList({QStringLiteral("--page"), QStringLiteral("processes"), QStringLiteral("--section"), QStringLiteral("all")}));
        const auto cwd = spy.first().at(1).toString();
        QCOMPARE(cwd, QStringLiteral("/home"));
    }
};

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "sublaunch") == 0) {
            QCoreApplication app(argc, argv);
            AppInstance secondary;
            const auto claim = secondary.claim({QStringLiteral("--page"), QStringLiteral("processes"), QStringLiteral("--section"), QStringLiteral("all")}, QStringLiteral("/home"));
            return claim == AppInstance::Claim::Forwarded ? 0 : 1;
        }
    }
    QCoreApplication app(argc, argv);
    TestSingleInstance tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_single_instance.moc"
