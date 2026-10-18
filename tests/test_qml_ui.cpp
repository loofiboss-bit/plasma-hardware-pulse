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
#include <QJSValue>

class ProcessControllerFixture : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList allProcesses READ allProcesses NOTIFY allProcessesChanged)
public:
    QVariantList allProcesses() const { return m_rows; }
    void setProcesses(const QVariantList &rows) { m_rows = rows; Q_EMIT allProcessesChanged(); }
Q_SIGNALS:
    void allProcessesChanged();
private:
    QVariantList m_rows;
};

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
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
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
            {"navigatePower", "powerPage"},
            {"navigateProcesses", "processesPage"},
            {"navigateStorage", "storageNetworkPage"}
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

        // Verify dedicated SettingsPage navigation and dialogs
        QVERIFY(QMetaObject::invokeMethod(window, "switchToPage", Q_ARG(QVariant, QStringLiteral("settings")), Q_ARG(QVariant, QString())));
        QCoreApplication::processEvents();
        QCOMPARE(window->property("currentPageName").toString(), QStringLiteral("settings"));

        auto *aboutDialog = window->findChild<QObject *>(QStringLiteral("aboutDialog"));
        QVERIFY(aboutDialog);
        QVERIFY(QMetaObject::invokeMethod(window, "openAboutDialog"));
        QTRY_VERIFY(aboutDialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(aboutDialog, "close"));

        auto *shortcutsDialog = window->findChild<QObject *>(QStringLiteral("shortcutsDialog"));
        QVERIFY(shortcutsDialog);
        QVERIFY(QMetaObject::invokeMethod(window, "openShortcutsDialog"));
        QTRY_VERIFY(shortcutsDialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(shortcutsDialog, "close"));

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
        QVERIFY(controller->property("incidentHistory").isValid());
        QTRY_VERIFY_WITH_TIMEOUT(!controller->property("historySamples").toList().isEmpty(), 5000);
        QVERIFY(controller->property("historyMaxWatts").toDouble() >= 60.0);
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

    void testProcessInspectionIdentityAndFiltering()
    {
        QQmlEngine engine;
        addImportPaths(engine);
        ProcessControllerFixture fixture;
        auto row = [](int pid, int start, const QString &name, const QString &owner, double cpu, double memory) {
            return QVariantMap{{QStringLiteral("pid"), pid}, {QStringLiteral("startTimeTicks"), start},
                {QStringLiteral("name"), name}, {QStringLiteral("owner"), owner},
                {QStringLiteral("cpuPercent"), cpu}, {QStringLiteral("rssMegabytes"), memory},
                {QStringLiteral("state"), QStringLiteral("S")}, {QStringLiteral("observedAtMs"), 1700000000000LL}};
        };
        QVariantMap editor = row(10, 100, QStringLiteral("editor"), QStringLiteral("alice"), 10, 400);
        const QVariantMap compiler = row(20, 200, QStringLiteral("compiler"), QStringLiteral("bob"), 90, 80);
        fixture.setProcesses({editor, compiler});
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(PULSE_APP_QML "/ProcessesPage.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> root(component.createWithInitialProperties({
            {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&fixture))}}));
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *page = qobject_cast<QQuickItem *>(root.data());
        QVERIFY(page);
        QQuickWindow window;
        window.resize(600, 700);
        page->setParentItem(window.contentItem());
        page->setSize(QSizeF(window.size()));
        window.show();
        QTest::qWait(80);
        const auto propertyValue = [&root](const char *name) {
            const QVariant value = root->property(name);
            return value.metaType() == QMetaType::fromType<QJSValue>() ? value.value<QJSValue>().toVariant() : value;
        };
        const auto visibleRows = [&propertyValue]() { return propertyValue("visibleProcesses").toList(); };
        QCOMPARE(visibleRows().size(), 2);
        QCOMPARE(visibleRows().first().toMap().value(QStringLiteral("pid")).toInt(), 20);
        auto *search = findVisualChild(page, QStringLiteral("processSearch"));
        auto *sort = findVisualChild(page, QStringLiteral("processSort"));
        QVERIFY(search);
        QVERIFY(sort);
        search->setProperty("text", QStringLiteral("alice"));
        QCOMPARE(visibleRows().size(), 1);
        QCOMPARE(visibleRows().first().toMap().value(QStringLiteral("pid")).toInt(), 10);
        search->setProperty("text", QStringLiteral("20"));
        QCOMPARE(visibleRows().first().toMap().value(QStringLiteral("pid")).toInt(), 20);
        search->setProperty("text", QString());
        sort->setProperty("currentIndex", 1);
        QCOMPARE(visibleRows().first().toMap().value(QStringLiteral("pid")).toInt(), 10);
        auto *editorButton = findVisualChild(page, QStringLiteral("processRow-10:100"));
        QTRY_VERIFY((editorButton = findVisualChild(page, QStringLiteral("processRow-10:100"))));
        QVERIFY(QMetaObject::invokeMethod(editorButton, "click"));
        QCOMPARE(root->property("selectedIdentity").toString(), QStringLiteral("10:100"));
        editor[QStringLiteral("cpuPercent")] = 99;
        fixture.setProcesses({compiler, editor});
        QCOMPARE(root->property("selectedIdentity").toString(), QStringLiteral("10:100"));
        QCOMPARE(propertyValue("selectedProcess").toMap().value(QStringLiteral("cpuPercent")).toDouble(), 99.0);
        // A reused PID must never silently select a different process.
        fixture.setProcesses({row(10, 999, QStringLiteral("replacement"), QStringLiteral("alice"), 100, 1)});
        QVERIFY(propertyValue("selectedProcess").isNull());
        QCOMPARE(root->property("selectedIdentity").toString(), QStringLiteral("10:100"));
        QCOMPARE(propertyValue("lastSelection").toMap().value(QStringLiteral("name")).toString(), QStringLiteral("editor"));
        QCOMPARE(propertyValue("lastSelection").toMap().value(QStringLiteral("cpuPercent")).toDouble(), 99.0);
        search->setProperty("text", QStringLiteral("does-not-exist"));
        QCOMPARE(visibleRows().size(), 0);
        page->setParentItem(nullptr);
    }

    void testSessionDialogsAndUnsavedClose()
    {
        QQmlApplicationEngine engine;
        addImportPaths(engine);
        engine.load(QUrl::fromLocalFile(QStringLiteral(PULSE_APP_QML "/Main.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->show();
        window->requestActivate();
        QTest::qWait(100);
        auto *openDialog = window->findChild<QObject *>(QStringLiteral("openSessionDialog"));
        auto *saveDialog = window->findChild<QObject *>(QStringLiteral("saveSessionDialog"));
        auto *unsavedDialog = window->findChild<QObject *>(QStringLiteral("unsavedInvestigationDialog"));
        QVERIFY(openDialog);
        QVERIFY(saveDialog);
        QVERIFY(unsavedDialog);
        QTest::keyClick(window, Qt::Key_O, Qt::ControlModifier);
        QTRY_VERIFY(openDialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(openDialog, "reject"));
        QTest::keyClick(window, Qt::Key_S, Qt::ControlModifier | Qt::ShiftModifier);
        QTRY_VERIFY(saveDialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(saveDialog, "reject"));
        auto *controller = window->findChild<QObject *>(QStringLiteral("telemetryController"));
        QVERIFY(controller);
        QTRY_VERIFY_WITH_TIMEOUT(!controller->property("historySamples").toList().isEmpty(), 5000);
        QVERIFY(QMetaObject::invokeMethod(controller, "markProblemNow"));
        QTRY_VERIFY(controller->property("unsavedInvestigation").toBool());
        QVERIFY(!window->close());
        QTRY_VERIFY(unsavedDialog->property("visible").toBool());
        QVERIFY(window->isVisible());
        QVERIFY(QMetaObject::invokeMethod(unsavedDialog, "reject"));
        QVERIFY(window->isVisible());
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
