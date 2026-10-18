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
#include <memory>

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

    if (isSmokeTest) QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
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
    QCommandLineOption pageOpt(QStringLiteral("page"), QStringLiteral("Initial view to open (overview, flight, pressure, power, processes, storage; devices is an alias)."), QStringLiteral("name"));
    QCommandLineOption sectionOpt(QStringLiteral("section"), QStringLiteral("Section to focus within the initial view."), QStringLiteral("name"));
    QCommandLineOption sessionOpt(QStringLiteral("session"), QStringLiteral("Open a saved Pulse session."), QStringLiteral("path"));
    QCommandLineOption incidentOpt(QStringLiteral("incident"), QStringLiteral("Open an incident from the shared collector."), QStringLiteral("id"));
    parser.addOption(sessionOpt);
    parser.addOption(incidentOpt);
    parser.addOption(smokeOpt);
    parser.addOption(screenshotOpt);
    parser.addOption(pageOpt);
    parser.addOption(sectionOpt);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    QQmlApplicationEngine engine;
    const QString appDir = QCoreApplication::applicationDirPath();
    for (const QString &relPath : {
             QStringLiteral("/../lib/qml"),
             QStringLiteral("/../lib64/qml"),
             QStringLiteral("/../lib/qt6/qml"),
             QStringLiteral("/../lib64/qt6/qml"),
         }) {
        const QString candidate = QDir::cleanPath(appDir + relPath);
        if (QDir(candidate).exists()) {
            engine.addImportPath(candidate);
        }
    }
    engine.addImportPath(appDir);

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

    const QString page = LoofiPulse::PulseTelemetryController::normalizedPageName(parser.value(pageOpt));
    engine.rootObjects().constFirst()->setProperty("initialSectionName",
        LoofiPulse::PulseTelemetryController::validatedSectionName(page, parser.value(sectionOpt)));
    engine.rootObjects().constFirst()->setProperty("initialPageName", page);

    if (parser.isSet(sessionOpt)) {
        QMetaObject::invokeMethod(engine.rootObjects().constFirst(), "openSession", Q_ARG(QVariant, parser.value(sessionOpt)));
    }
    if (parser.isSet(incidentOpt)) {
        auto *root = engine.rootObjects().constFirst();
        auto *telemetry = root->findChild<QObject *>(QStringLiteral("telemetryController"));
        const QString incident = parser.value(incidentOpt);
        auto opened = std::make_shared<bool>(false);
        auto open = [root, telemetry, incident, opened] {
            if (!telemetry || *opened || !telemetry->property("collectorState").toString().startsWith(QStringLiteral("Live"))) return;
            *opened = true;
            QMetaObject::invokeMethod(root, "openIncident", Q_ARG(QVariant, incident));
        };
        auto *activationTimer = new QTimer(root);
        activationTimer->setInterval(100);
        QObject::connect(activationTimer, &QTimer::timeout, root, [open, opened, activationTimer] { open(); if (*opened) activationTimer->stop(); });
        activationTimer->start();
    }

    if (parser.isSet(smokeOpt)) {
        LoofiPulse::PulseTelemetryController probe(QDBusConnection::systemBus());
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
