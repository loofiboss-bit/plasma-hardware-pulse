#include <KLocalizedString>
#include <QCoreApplication>
#include <QDir>
#include <QLocale>
#include <QtTest>

class TestI18n : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        const QString buildLocaleDir = QStringLiteral(PULSE_BUILD_LOCALE_DIR);
        KLocalizedString::addDomainLocaleDir("loofi-pulse", buildLocaleDir);
        KLocalizedString::addDomainLocaleDir("plasma_applet_com.github.loofi.hardwarepulse", buildLocaleDir);
        KLocalizedString::setApplicationDomain("loofi-pulse");
        KLocalizedString::setLanguages({QStringLiteral("sv")});
    }

    void testSwedishTranslationsAppDomain()
    {
        // Test key application translations
        const QString overview = i18n("Overview");
        const QString flight = i18n("Flight Recorder");
        const QString settings = i18n("Settings");
        const QString sysmon = i18n("System Monitor");
        const QString throttle = i18n("Critical Throttle");
        const QString nominal = i18n("Nominal");

        QCOMPARE(overview, QStringLiteral("Översikt"));
        QCOMPARE(flight, QStringLiteral("Färdskrivare"));
        QCOMPARE(settings, QStringLiteral("Inställningar"));
        QCOMPARE(sysmon, QStringLiteral("Systemövervakare"));
        QCOMPARE(throttle, QStringLiteral("Kritisk strypning"));
        QCOMPARE(nominal, QStringLiteral("Nominell"));
    }

    void testSwedishTranslationsAppletDomain()
    {
        const QString openPulse = i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Open Loofi Pulse");
        const QString markProblem = i18nd("plasma_applet_com.github.loofi.hardwarepulse", "Mark Problem Now");

        QCOMPARE(openPulse, QStringLiteral("Öppna Loofi Pulse"));
        QCOMPARE(markProblem, QStringLiteral("Markera problem nu"));
    }
};

QTEST_MAIN(TestI18n)
#include "test_i18n.moc"
