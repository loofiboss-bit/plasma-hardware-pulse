#include "pulsetelemetrycontroller.h"

#include <KAboutData>
#include <KLocalizedString>
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

int main(int argc, char *argv[])
{
    bool isSmokeTest = false;
    for (int i = 1; i < argc; ++i) {
        if (QLatin1StringView(argv[i]) == QLatin1StringView("--smoke-test")) {
            isSmokeTest = true;
            break;
        }
    }

    if (isSmokeTest && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    }

    QApplication app(argc, argv);
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    KLocalizedString::setApplicationDomain("loofi-pulse");
    KLocalizedString::setLanguages({QStringLiteral("en_US"), QStringLiteral("en")});

    KAboutData aboutData(
        QStringLiteral("loofi-pulse"),
        QStringLiteral("Loofi Pulse"),
        QStringLiteral(LOOFI_PULSE_VERSION),
        QStringLiteral("KDE Plasma 6 hardware health and pressure monitor"),
        KAboutLicense::GPL_V3,
        QStringLiteral("© 2026 Loofi"));
    KAboutData::setApplicationData(aboutData);
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("com.github.loofi.hardwarepulse"),
                                       QIcon::fromTheme(QStringLiteral("utilities-system-monitor"))));

    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    QCommandLineOption smokeOpt(QStringLiteral("smoke-test"), QStringLiteral("Run automated QML & telemetry verification and exit."));
    QCommandLineOption screenshotOpt(QStringLiteral("screenshot"), QStringLiteral("Render window to PNG file and exit."), QStringLiteral("path"));
    QCommandLineOption pageOpt(QStringLiteral("page"), QStringLiteral("Initial view to open (overview, flight, pressure, power)."), QStringLiteral("name"));
    parser.addOption(smokeOpt);
    parser.addOption(screenshotOpt);
    parser.addOption(pageOpt);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    QQmlApplicationEngine engine;
    engine.addImportPath(QCoreApplication::applicationDirPath());

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(1); },
        Qt::QueuedConnection);

    engine.load(QUrl(QStringLiteral("qrc:/loofipulse/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    if (parser.isSet(pageOpt)) {
        engine.rootObjects().constFirst()->setProperty("initialPageName", parser.value(pageOpt));
    }

    if (parser.isSet(smokeOpt)) {
        LoofiPulse::PulseTelemetryController probe;
        probe.refreshNow();
        if (probe.verdictHeadline().isEmpty()) {
            qCritical("Smoke test could not obtain a controller status headline.");
            return 2;
        }
        QTimer::singleShot(150, &app, &QCoreApplication::quit);
    } else if (parser.isSet(screenshotOpt)) {
        const QString outPath = parser.value(screenshotOpt);
        auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QTimer::singleShot(2200, &app, [win, outPath]() {
            if (win) {
                const QImage img = win->grabWindow();
                img.save(outPath);
            }
            QCoreApplication::quit();
        });
    }

    return app.exec();
}
