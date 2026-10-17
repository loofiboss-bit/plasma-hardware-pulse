#include <QDir>
#include <QFont>
#include <QGuiApplication>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QSignalSpy>

class TestQmlUi : public QObject
{
    Q_OBJECT

private:
    static QString artifacts()
    {
        const QString path = QString::fromLocal8Bit(qgetenv("PULSE_TEST_ARTIFACT_DIR"));
        QDir().mkpath(path);
        return path;
    }

    static void addImportPaths(QQmlEngine &engine)
    {
        engine.addImportPath(QStringLiteral(PULSE_BUILD_BIN));
        engine.addImportPath(QStringLiteral(PULSE_APP_QML));
    }

    static QQuickItem *currentPage(QQuickWindow *window)
    {
        auto *stack = qvariant_cast<QObject *>(window->property("pageStack"));
        return stack ? qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(stack->property("currentItem"))) : nullptr;
    }

    static QQuickItem *findVisualChild(QQuickItem *root, const QString &name)
    {
        if (!root) {
            return nullptr;
        }
        if (root->objectName() == name) {
            return root;
        }
        if (auto *direct = root->findChild<QQuickItem *>(name)) {
            return direct;
        }
        const auto children = root->childItems();
        for (QQuickItem *child : children) {
            if (auto *found = findVisualChild(child, name)) {
                return found;
            }
        }
        return nullptr;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("LoofiPulseTests"));
        QCoreApplication::setOrganizationDomain(QStringLiteral("tests.invalid"));
    }

    void testAppNavigationKeyboardAndResponsiveRendering()
    {
        QQmlApplicationEngine engine;
        QStringList runtimeErrors;
        QObject::connect(&engine, &QQmlEngine::warnings, &engine, [&runtimeErrors](const QList<QQmlError> &warnings) {
            for (const QQmlError &warning : warnings) {
                const QString message = warning.toString();
                if (message.contains(QStringLiteral("TypeError")) || message.contains(QStringLiteral("ReferenceError"))
                    || message.contains(QStringLiteral("required property")) || message.contains(QStringLiteral("Cannot assign"))
                    || message.contains(QStringLiteral("Unable to assign")) || message.contains(QStringLiteral("Binding loop"))) {
                    runtimeErrors.append(message);
                }
            }
        });
        addImportPaths(engine);
        engine.load(QUrl::fromLocalFile(QStringLiteral(PULSE_APP_QML "/Main.qml")));
        QVERIFY2(!engine.rootObjects().isEmpty(), "Main QML must instantiate");
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QCOMPARE(window->objectName(), QStringLiteral("pulseWindow"));

        window->setWidth(600);
        window->setHeight(700);
        const QFont originalFont = QGuiApplication::font();
        QFont largeFont = originalFont;
        largeFont.setPointSize(qMax(18, largeFont.pointSize() + 6));
        QGuiApplication::setFont(largeFont);
        window->show();
        QTest::qWait(120);

        const struct { const char *action; const char *page; } pages[] = {
            {"navigateOverview", "overviewPage"},
            {"navigateHistory", "historyPage"},
            {"navigatePressure", "pressurePage"},
            {"navigatePower", "powerPage"}
        };
        for (const auto &entry : pages) {
            QObject *action = window->findChild<QObject *>(QLatin1String(entry.action));
            QVERIFY2(action, entry.action);
            QVERIFY(QMetaObject::invokeMethod(action, "trigger"));
            QCoreApplication::processEvents();
            auto *page = currentPage(window);
            QVERIFY2(page, entry.page);
            QCOMPARE(page->objectName(), QLatin1String(entry.page));
            QVERIFY2(page->width() <= window->width() + 1, "Current page extends beyond narrow viewport");
            if (QLatin1String(entry.page) == QStringLiteral("overviewPage")) {
                auto *details = findVisualChild(page, QStringLiteral("measurementDetailsButton"));
                QVERIFY(details);
                details->forceActiveFocus();
                QTest::keyClick(window, Qt::Key_Space);
                QVERIFY(details->property("checked").toBool());
            }
        }

        QObject *overviewAction = window->findChild<QObject *>(QStringLiteral("navigateOverview"));
        QVERIFY(QMetaObject::invokeMethod(overviewAction, "trigger"));
        QCoreApplication::processEvents();
        QObject *overviewPage = currentPage(window);
        QVERIFY(overviewPage);
        QVERIFY(QMetaObject::invokeMethod(overviewPage, "navigateRequested", Q_ARG(QString, QStringLiteral("power")), Q_ARG(QString, QStringLiteral("gpu"))));
        QCoreApplication::processEvents();
        QVERIFY(window->findChild<QQuickItem *>(QStringLiteral("powerPage")) != nullptr);
        auto *gpuSection = findVisualChild(currentPage(window), QStringLiteral("section_gpu"));
        QVERIFY(gpuSection);
        QTRY_VERIFY(gpuSection->hasActiveFocus());
        QVERIFY(QMetaObject::invokeMethod(window, "switchToPage", Q_ARG(QVariant, QStringLiteral("pressure")), Q_ARG(QVariant, QStringLiteral("network"))));
        QCoreApplication::processEvents();
        auto *networkSection = findVisualChild(currentPage(window), QStringLiteral("section_network"));
        QVERIFY(networkSection);
        QTRY_VERIFY(networkSection->hasActiveFocus());
        QVERIFY(QMetaObject::invokeMethod(window, "switchToPage", Q_ARG(QVariant, QStringLiteral("power")), Q_ARG(QVariant, QStringLiteral("invalid-section"))));
        QCoreApplication::processEvents();
        QVERIFY(window->findChild<QQuickItem *>(QStringLiteral("powerPage")));
        auto *settingsAction = window->findChild<QObject *>(QStringLiteral("openSettings"));
        QVERIFY(settingsAction);
        QVERIFY(QMetaObject::invokeMethod(settingsAction, "trigger"));
        QCoreApplication::processEvents();
        auto *settingsDialog = window->findChild<QObject *>(QStringLiteral("settingsDialog"));
        QVERIFY(settingsDialog);
        QVERIFY(settingsDialog->property("visible").toBool());
        QVERIFY(window->findChild<QObject *>(QStringLiteral("samplingIntervalSetting")));
        QVERIFY(QMetaObject::invokeMethod(settingsDialog, "close"));

        QObject *historyAction = window->findChild<QObject *>(QStringLiteral("navigateHistory"));
        QVERIFY(QMetaObject::invokeMethod(historyAction, "trigger"));
        QCoreApplication::processEvents();
        auto *timeline = findVisualChild(currentPage(window), QStringLiteral("section_timeline"));
        QVERIFY(timeline);
        timeline->forceActiveFocus();
        QVERIFY(timeline->hasActiveFocus());
        QObject *historyPage = currentPage(window);
        QVERIFY(historyPage);
        QObject *controller = qvariant_cast<QObject *>(historyPage->property("controller"));
        QVERIFY(controller);
        QVERIFY(controller->property("historyMaxWatts").toDouble() >= 60.0);
        QVERIFY(controller->property("incidentHistory").isValid());
        QTest::keyClick(window, Qt::Key_Left);
        QVERIFY(controller->property("selectedHistorySampleId").toULongLong() > 0);
        QTest::keyClick(window, Qt::Key_Right);

        auto *clearHistoryBtn = window->findChild<QQuickItem *>(QStringLiteral("clearHistoryButton"));
        QVERIFY(clearHistoryBtn);
        controller->setProperty("historyRetentionSeconds", 900);
        QCOMPARE(controller->property("historyRetentionSeconds").toInt(), 900);
        controller->setProperty("historyRetentionSeconds", 300);
        QCOMPARE(controller->property("historyRetentionSeconds").toInt(), 300);

        QPalette lightPalette = QGuiApplication::palette();
        lightPalette.setColor(QPalette::Window, Qt::white);
        lightPalette.setColor(QPalette::WindowText, Qt::black);
        lightPalette.setColor(QPalette::Base, Qt::white);
        lightPalette.setColor(QPalette::Text, Qt::black);
        const QPalette originalPalette = QGuiApplication::palette();
        QGuiApplication::setPalette(lightPalette);
        QCoreApplication::processEvents();
        const QImage image = window->grabWindow();
        QVERIFY(!image.isNull());
        QVERIFY(image.save(artifacts() + QStringLiteral("/app-history-narrow-large-font.png")));
        window->setWidth(1160);
        window->setHeight(850);
        QGuiApplication::setFont(originalFont);
        QTest::qWait(80);
        QVERIFY(window->grabWindow().save(artifacts() + QStringLiteral("/app-history-light.png")));

        QPalette darkPalette = originalPalette;
        darkPalette.setColor(QPalette::Window, QColor(QStringLiteral("#20232a")));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(QStringLiteral("#15171c")));
        darkPalette.setColor(QPalette::Text, Qt::white);
        QGuiApplication::setPalette(darkPalette);
        QCoreApplication::processEvents();
        QVERIFY(window->grabWindow().save(artifacts() + QStringLiteral("/app-history-dark.png")));
        QGuiApplication::setPalette(originalPalette);
        QGuiApplication::setFont(originalFont);
        QVERIFY2(runtimeErrors.isEmpty(), qPrintable(runtimeErrors.join(QLatin1Char('\n'))));
    }

    void testPanelPresetsPreserveCustomMetrics()
    {
        QQmlEngine engine;
        addImportPaths(engine);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(PULSE_WIDGET_QML "/configGeneral.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> config(component.create());
        QVERIFY2(config, qPrintable(component.errorString()));
        config->setProperty("cfg_showCpuLoadInPanel", true);
        config->setProperty("cfg_showGpuInPanel", false);
        QVERIFY(QMetaObject::invokeMethod(config.data(), "applyPreset", Q_ARG(QVariant, 3)));
        QVERIFY(config->property("cfg_showCpuLoadInPanel").toBool());
        QVERIFY(!config->property("cfg_showGpuInPanel").toBool());
        QVERIFY(QMetaObject::invokeMethod(config.data(), "applyPreset", Q_ARG(QVariant, 0)));
        QVERIFY(!config->property("cfg_showWattsInPanel").toBool());
        QVERIFY(!config->property("cfg_showPsiInPanel").toBool());
        QVERIFY(QMetaObject::invokeMethod(config.data(), "applyPreset", Q_ARG(QVariant, 1)));
        QVERIFY(config->property("cfg_showWattsInPanel").toBool());
        QVERIFY(config->property("cfg_showPsiInPanel").toBool());
        QVERIFY(!config->property("cfg_showGpuInPanel").toBool());
        QVERIFY(QMetaObject::invokeMethod(config.data(), "applyPreset", Q_ARG(QVariant, 2)));
        QVERIFY(config->property("cfg_showGpuInPanel").toBool());
        QVERIFY(config->property("cfg_showIoInPanel").toBool());
        QCOMPARE(config->property("cfg_panelPreset").toString(), QStringLiteral("detailed"));
    }

    void testWidgetComponentsLoad()
    {
        QQmlEngine engine;
        QStringList runtimeErrors;
        QObject::connect(&engine, &QQmlEngine::warnings, &engine, [&runtimeErrors](const QList<QQmlError> &warnings) {
            for (const QQmlError &warning : warnings) {
                const QString message = warning.toString();
                if (message.contains(QStringLiteral("TypeError")) || message.contains(QStringLiteral("ReferenceError"))
                    || message.contains(QStringLiteral("required property")) || message.contains(QStringLiteral("Cannot assign"))
                    || message.contains(QStringLiteral("Unable to assign")) || message.contains(QStringLiteral("Binding loop"))) {
                    runtimeErrors.append(message);
                }
            }
        });
        addImportPaths(engine);
        QQmlComponent controllerComponent(&engine);
        controllerComponent.setData("import com.github.loofi.hardwarepulse 1.0\nPulseTelemetryController {}", QUrl());
        QVERIFY2(controllerComponent.isReady(), qPrintable(controllerComponent.errorString()));
        QScopedPointer<QObject> controller(controllerComponent.create());
        QVERIFY(controller);

        const QString widgetDir = QStringLiteral(PULSE_WIDGET_QML);
        for (const QString &name : {QStringLiteral("FullRepresentation"), QStringLiteral("CompactRepresentation")}) {
            QQmlComponent component(&engine, QUrl::fromLocalFile(widgetDir + QLatin1Char('/') + name + QStringLiteral(".qml")));
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));
            QVariantMap initial{{QStringLiteral("controller"), QVariant::fromValue(controller.data())}};
            if (name == QStringLiteral("CompactRepresentation")) initial.insert(QStringLiteral("expanded"), false);
            QScopedPointer<QObject> root(component.createWithInitialProperties(initial));
            QVERIFY2(root, qPrintable(component.errorString()));
            auto *item = qobject_cast<QQuickItem *>(root.data());
            QVERIFY(item);
            QQuickWindow window;
            window.setWidth(name == QStringLiteral("CompactRepresentation") ? 480 : 460);
            window.setHeight(name == QStringLiteral("CompactRepresentation") ? 55 : 620);
            item->setParentItem(window.contentItem());
            item->setWidth(window.width());
            item->setHeight(window.height());
            window.show();
            QTest::qWait(100);
            if (name == QStringLiteral("FullRepresentation")) {
                QCOMPARE(item->property("sparkChannel").toString(), QStringLiteral("temp"));
                QVERIFY(findVisualChild(item, QStringLiteral("sparkChannelCpuButton")) != nullptr);
                QVERIFY(findVisualChild(item, QStringLiteral("sparkChannelPsiButton")) != nullptr);
                QVERIFY(findVisualChild(item, QStringLiteral("sparkChannelPowerButton")) != nullptr);
                for (const QString &ch : {QStringLiteral("cpu"), QStringLiteral("psi"), QStringLiteral("power"), QStringLiteral("temp")}) {
                    item->setProperty("sparkChannel", ch);
                    QCoreApplication::processEvents();
                }
            } else if (name == QStringLiteral("CompactRepresentation")) {
                item->setProperty("showCpuLoadInPanel", true);
                item->setProperty("showGpuInPanel", true);
                item->setProperty("showRamInPanel", true);
                item->setProperty("showIoInPanel", true);
                QCoreApplication::processEvents();
                QVERIFY(!item->property("panelText").toString().contains(QStringLiteral("% PSI")));
                QVERIFY(item->property("panelText").toString().contains(QStringLiteral("Responsiveness")));
                auto *button = findVisualChild(item, QStringLiteral("panelStatusButton"));
                QVERIFY(button);
                QSignalSpy activation(root.data(), SIGNAL(activationRequested()));
                button->forceActiveFocus();
                QTest::keyClick(&window, Qt::Key_Space);
                QCOMPARE(activation.count(), 1);
                item->setProperty("verticalPanel", true);
                window.setWidth(60);
                window.setHeight(90);
                item->setWidth(60);
                item->setHeight(90);
                QTest::qWait(30);
                QVERIFY(window.grabWindow().save(artifacts() + QStringLiteral("/widget-vertical.png")));
                item->setProperty("verticalPanel", false);
                window.setWidth(360);
                item->setWidth(360);
            }
            const QImage image = window.grabWindow();
            QVERIFY(!image.isNull());
            QVERIFY(image.save(artifacts() + QStringLiteral("/widget-") + name.toLower() + QStringLiteral(".png")));
            item->setParentItem(nullptr);
        }
        QVERIFY2(runtimeErrors.isEmpty(), qPrintable(runtimeErrors.join(QLatin1Char('\n'))));
    }
};

QTEST_MAIN(TestQmlUi)
#include "test_qml_ui.moc"
