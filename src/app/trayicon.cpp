#include "trayicon.h"
#include "pulsetelemetrycontroller.h"

#include <KLocalizedString>
#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QMetaObject>
#include <QSystemTrayIcon>
#include <QWindow>

namespace LoofiPulse {

TrayIcon::TrayIcon(QObject *parent)
    : QObject(parent)
{
}

TrayIcon::~TrayIcon()
{
    if (m_tray) {
        m_tray->hide();
    }
}

bool TrayIcon::isAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

bool TrayIcon::isVisible() const
{
    return m_visible && m_tray && m_tray->isVisible();
}

void TrayIcon::setVisible(bool visible)
{
    if (m_visible == visible) return;
    m_visible = visible;
    if (m_tray && isAvailable()) {
        m_tray->setVisible(m_visible);
    }
    Q_EMIT visibleChanged();
}

void TrayIcon::setMinimizeToTrayOnClose(bool enable)
{
    if (m_minimizeToTrayOnClose == enable) return;
    m_minimizeToTrayOnClose = enable;
    Q_EMIT minimizeToTrayOnCloseChanged();
}

void TrayIcon::init(PulseTelemetryController *controller, QWindow *mainWindow)
{
    m_controller = controller;
    m_mainWindow = mainWindow;

    if (!isAvailable()) {
        return;
    }

    if (!m_tray) {
        m_tray = new QSystemTrayIcon(this);
        m_menu = new QMenu();

        m_actionToggleWindow = m_menu->addAction(i18n("Show Loofi Pulse"), this, &TrayIcon::toggleWindowVisibility);
        m_menu->addSeparator();

        m_actionMarkProblem = m_menu->addAction(i18n("Mark Problem Now"), [this]() {
            if (m_controller) m_controller->markProblemNow();
        });

        m_actionPause = m_menu->addAction(i18n("Pause Recording"), [this]() {
            if (m_controller) m_controller->setRecordingPaused(!m_controller->recordingPaused());
        });

        m_actionSnooze = m_menu->addAction(i18n("Snooze Notifications (15 min)"), [this]() {
            if (m_controller) m_controller->snoozeNotifications();
        });

        m_menu->addSeparator();
        m_actionQuit = m_menu->addAction(i18n("Quit Loofi Pulse"), this, &TrayIcon::quitRequested);

        m_tray->setContextMenu(m_menu);

        connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            onTrayActivated(static_cast<int>(reason));
        });
    }

    if (m_controller) {
        connect(m_controller, &PulseTelemetryController::telemetryUpdated, this, &TrayIcon::onTelemetryUpdated);
        connect(m_controller, &PulseTelemetryController::recordingPausedChanged, this, &TrayIcon::updateMenu);
        connect(m_controller, &PulseTelemetryController::temperatureUnitChanged, this, &TrayIcon::updateTooltipAndIcon);
    }

    updateTooltipAndIcon();
    updateMenu();

    if (m_visible) {
        m_tray->show();
    }
}

void TrayIcon::onTelemetryUpdated()
{
    updateTooltipAndIcon();
    updateMenu();
}

void TrayIcon::updateTooltipAndIcon()
{
    if (!m_tray || !m_controller) return;

    const int severity = m_controller->verdictSeverity();
    QString headline = m_controller->verdictHeadline();
    if (headline.isEmpty()) {
        headline = i18n("Hardware Health Monitor");
    }

    const bool connected = m_controller->collectorConnected();
    const QString unavailable = i18n("Unavailable");
    const QString cpu = connected && m_controller->cpuLoadAvailable()
        ? QString::number(qRound(m_controller->cpuLoadPercent())) + QStringLiteral("%") : unavailable;
    const bool isF = m_controller->temperatureUnit() == QStringLiteral("F");
    const double temp = isF ? m_controller->cpuPackageTemp() * 9.0 / 5.0 + 32.0 : m_controller->cpuPackageTemp();
    const QString temperature = connected && m_controller->cpuTempAvailable()
        ? QStringLiteral("%1°%2").arg(qRound(temp)).arg(isF ? QStringLiteral("F") : QStringLiteral("C")) : unavailable;
    const QString memory = connected && m_controller->memoryAvailable()
        ? QString::number(qRound(m_controller->ramUsedPercent())) + QStringLiteral("%") : unavailable;
    if (!connected) headline = i18n("Collector disconnected");
    const QString tooltip = i18n("Loofi Pulse — %1\nCPU: %2 · Temperature: %3 · Memory: %4", headline, cpu, temperature, memory);

    m_tray->setToolTip(tooltip);

    // Icon updates based on severity
    if (connected && severity == 3) {
        m_tray->setIcon(QIcon::fromTheme(QStringLiteral("dialog-error"),
                                         QIcon::fromTheme(QStringLiteral("utilities-system-monitor"))));
    } else if (connected && severity >= 1 && severity <= 2) {
        m_tray->setIcon(QIcon::fromTheme(QStringLiteral("dialog-warning"),
                                         QIcon::fromTheme(QStringLiteral("utilities-system-monitor"))));
    } else {
        m_tray->setIcon(QIcon::fromTheme(QStringLiteral("com.github.loofi.hardwarepulse"),
                                         QIcon::fromTheme(QStringLiteral("utilities-system-monitor"))));
    }
}

void TrayIcon::updateMenu()
{
    if (!m_menu || !m_controller) return;

    const bool winVisible = m_mainWindow && m_mainWindow->isVisible();
    if (m_actionToggleWindow) {
        m_actionToggleWindow->setText(winVisible ? i18n("Hide Loofi Pulse") : i18n("Show Loofi Pulse"));
    }

    if (m_actionPause) {
        m_actionPause->setText(m_controller->recordingPaused() ? i18n("Resume Recording") : i18n("Pause Recording"));
    }
}

void TrayIcon::onTrayActivated(int reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        toggleWindowVisibility();
    }
}

void TrayIcon::toggleWindowVisibility()
{
    if (!m_mainWindow) return;

    if (m_mainWindow->isVisible()) {
        m_mainWindow->hide();
    } else {
        QMetaObject::invokeMethod(m_mainWindow, "restoreFromBackground");
        m_mainWindow->show();
        m_mainWindow->raise();
        m_mainWindow->requestActivate();
    }
    updateMenu();
}

void TrayIcon::showInformation(const QString &title, const QString &message, int timeoutMs)
{
    if (m_tray && isAvailable()) {
        m_tray->showMessage(title, message, QSystemTrayIcon::Information, timeoutMs);
    }
}

} // namespace LoofiPulse
