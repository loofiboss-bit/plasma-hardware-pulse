#include "appinstance.h"
#include "pulsetelemetrycontroller.h"
#include "trayicon.h"

#include <KAboutData>
#include <KCrash>
#include <KLocalizedContext>
#include <KLocalizedString>
#include <KWindowSystem>
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>
#include <memory>

namespace {
struct LaunchOptions {
    QCommandLineOption smoke{QStringLiteral("smoke-test"), QStringLiteral("Run automated QML & telemetry verification and exit.")};
    QCommandLineOption screenshot{QStringLiteral("screenshot"), QStringLiteral("Render window to PNG file and exit."), QStringLiteral("path")};
    QCommandLineOption page{QStringLiteral("page"), QStringLiteral("Initial view to open (overview, flight, pressure, power, processes, storage, settings; devices is an alias)."), QStringLiteral("name")};
    QCommandLineOption section{QStringLiteral("section"), QStringLiteral("Section to focus within the initial view."), QStringLiteral("name")};
    QCommandLineOption session{QStringLiteral("session"), QStringLiteral("Open a saved Pulse session."), QStringLiteral("path")};
    QCommandLineOption incident{QStringLiteral("incident"), QStringLiteral("Open an incident from the shared collector."), QStringLiteral("id")};
    void addTo(QCommandLineParser &parser) const
    {
        parser.addOption(session); parser.addOption(incident); parser.addOption(smoke);
        parser.addOption(screenshot); parser.addOption(page); parser.addOption(section);
    }
};

// Applies navigation arguments to the window. Used for the first launch and for
// arguments forwarded by later launches.
void applyArguments(QObject *root, const QCommandLineParser &parser, const LaunchOptions &options, const QString &workingDirectory, bool navigate)
{
    if (navigate && (parser.isSet(options.page) || parser.isSet(options.section))) {
        const QString page = LoofiPulse::PulseTelemetryController::normalizedPageName(parser.value(options.page));
        const QString section = LoofiPulse::PulseTelemetryController::validatedSectionName(page, parser.value(options.section));
        QMetaObject::invokeMethod(root, "navigateTo", Q_ARG(QVariant, page), Q_ARG(QVariant, section));
    }
    if (parser.isSet(options.session)) {
        QString path = parser.value(options.session);
        if (!path.startsWith(QStringLiteral("file:")) && QDir::isRelativePath(path))
            path = QDir(workingDirectory).absoluteFilePath(path);
        QMetaObject::invokeMethod(root, "openSession", Q_ARG(QVariant, path));
    }
    if (parser.isSet(options.incident)) {
        auto *telemetry = root->findChild<QObject *>(QStringLiteral("telemetryController"));
        const QString incident = parser.value(options.incident);
        auto opened = std::make_shared<bool>(false);
        auto open = [root, telemetry, incident, opened] {
            if (!telemetry || *opened || !telemetry->property("collectorState").toString().startsWith(QStringLiteral("Live"))) return;
            *opened = true;
            QMetaObject::invokeMethod(root, "openIncident", Q_ARG(QVariant, incident));
        };
        auto *activationTimer = new QTimer(root);
        activationTimer->setInterval(100);
        QObject::connect(activationTimer, &QTimer::timeout, root, [open, opened, activationTimer] {
            open();
            if (*opened) activationTimer->stop();
        });
        activationTimer->start();
    }
}
}

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
    if (isSmokeTest) {
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
        KLocalizedString::setLanguages({QStringLiteral("en_US"), QStringLiteral("en")});
    }
    KLocalizedString::setApplicationDomain("loofi-pulse");

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
    const LaunchOptions options;
    options.addTo(parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    const bool automated = parser.isSet(options.smoke) || parser.isSet(options.screenshot);
    LoofiPulse::AppInstance instance;
    if (!automated && qgetenv("LOOFI_PULSE_ALLOW_MULTIPLE") != "1") {
        if (instance.claim(app.arguments(), QDir::currentPath()) == LoofiPulse::AppInstance::Claim::Forwarded)
            return 0;
    }
    if (!automated) KCrash::initialize();

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
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
QT_WARNING_POP

    LoofiPulse::TrayIcon trayIcon;
    engine.rootContext()->setContextProperty(QStringLiteral("appTray"), &trayIcon);

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
    auto *root = engine.rootObjects().constFirst();

    if (!automated) {
        auto *window = qobject_cast<QQuickWindow *>(root);
        auto *telemetry = root->findChild<LoofiPulse::PulseTelemetryController *>(QStringLiteral("telemetryController"));
        trayIcon.init(telemetry, window);
        root->setProperty("tray", QVariant::fromValue(&trayIcon));
    }

    const QString page = LoofiPulse::PulseTelemetryController::normalizedPageName(parser.value(options.page));
    root->setProperty("initialSectionName",
        LoofiPulse::PulseTelemetryController::validatedSectionName(page, parser.value(options.section)));
    root->setProperty("initialPageName", page);

    // Page and section were applied through the initial properties above.
    applyArguments(root, parser, options, QDir::currentPath(), false);

    QObject::connect(&instance, &LoofiPulse::AppInstance::activateRequested, root,
        [root, &options](const QStringList &arguments, const QString &workingDirectory, const QString &token) {
            if (auto *window = qobject_cast<QQuickWindow *>(root)) {
                QMetaObject::invokeMethod(root, "restoreFromBackground");
                if (!token.isEmpty()) KWindowSystem::setCurrentXdgActivationToken(token);
                KWindowSystem::activateWindow(window);
                window->raise();
                window->requestActivate();
            }
            QCommandLineParser forwarded;
            options.addTo(forwarded);
            // Unknown options (for example KAboutData flags) must not block known navigation.
            if (arguments.isEmpty()) return;
            forwarded.parse(arguments);
            applyArguments(root, forwarded, options, workingDirectory, true);
        });

    if (parser.isSet(options.smoke)) {
        LoofiPulse::PulseTelemetryController probe(QDBusConnection::systemBus());
        probe.refreshNow();
        if (probe.verdictHeadline().isEmpty()) {
            qCritical("Smoke test could not obtain a controller status headline.");
            return 2;
        }
        QTimer::singleShot(150, &app, &QCoreApplication::quit);
    } else if (parser.isSet(options.screenshot)) {
        const QString outPath = parser.value(options.screenshot);
        auto *win = qobject_cast<QQuickWindow *>(root);
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
