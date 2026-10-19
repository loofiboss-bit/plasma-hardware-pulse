#include "trayicon.h"
#include "pulsetelemetrycontroller.h"

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

        m_actionToggleWindow = m_menu->addAction(QStringLiteral("Show Loofi Pulse"), this, &TrayIcon::toggleWindowVisibility);
        m_menu->addSeparator();

        m_actionMarkProblem = m_menu->addAction(QStringLiteral("Mark Problem Now"), [this]() {
            if (m_controller) m_controller->markProblemNow();
        });

        m_actionPause = m_menu->addAction(QStringLiteral("Pause Recording"), [this]() {
            if (m_controller) m_controller->setRecordingPaused(!m_controller->recordingPaused());
        });

        m_actionSnooze = m_menu->addAction(QStringLiteral("Snooze Notifications (15 min)"), [this]() {
            if (m_controller) m_controller->snoozeNotifications();
        });

        m_menu->addSeparator();
        m_actionQuit = m_menu->addAction(QStringLiteral("Quit Loofi Pulse"), qApp, &QCoreApplication::quit);

        m_tray->setContextMenu(m_menu);

        connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            onTrayActivated(static_cast<int>(reason));
        });
    }

    if (m_controller) {
        connect(m_controller, &PulseTelemetryController::telemetryUpdated, this, &TrayIcon::onTelemetryUpdated);
        connect(m_controller, &PulseTelemetryController::recordingPausedChanged, this, &TrayIcon::updateMenu);
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
        headline = QStringLiteral("Hardware Health Monitor");
    }

    const int cpuPct = qRound(m_controller->cpuLoadPercent());
    const double rawTemp = m_controller->cpuPackageTemp();
    const bool isF = m_controller->temperatureUnit() == QStringLiteral("F");
    const double temp = isF ? (rawTemp * 9.0 / 5.0 + 32.0) : rawTemp;
    const QString tempStr = QStringLiteral("%1°%2").arg(qRound(temp)).arg(isF ? QStringLiteral("F") : QStringLiteral("C"));

    const double ramUsed = m_controller->ramUsedGib();
    const double ramTotal = m_controller->ramTotalGib();
    const int ramPct = ramTotal > 0.0 ? qRound((ramUsed / ramTotal) * 100.0) : 0;

    QString statusText;
    switch (severity) {
    case 0: statusText = QStringLiteral("Nominal"); break;
    case 1: statusText = QStringLiteral("Elevated"); break;
    case 2: statusText = QStringLiteral("Bottleneck"); break;
    case 3: statusText = QStringLiteral("Critical Throttle"); break;
    default: statusText = QStringLiteral("Observing"); break;
    }

    const QString tooltip = QStringLiteral("Loofi Pulse — %1\nCPU: %2% (%3) · RAM: %4% (%5 / %6 GiB)\nStatus: %7")
                                .arg(headline)
                                .arg(cpuPct)
                                .arg(tempStr)
                                .arg(ramPct)
                                .arg(QString::number(ramUsed, 'f', 1))
                                .arg(QString::number(ramTotal, 'f', 1))
                                .arg(statusText);

    m_tray->setToolTip(tooltip);

    // Icon updates based on severity
    if (severity >= 3) {
        m_tray->setIcon(QIcon::fromTheme(QStringLiteral("dialog-error"),
                                         QIcon::fromTheme(QStringLiteral("utilities-system-monitor"))));
    } else if (severity >= 1) {
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
        m_actionToggleWindow->setText(winVisible ? QStringLiteral("Hide Loofi Pulse") : QStringLiteral("Show Loofi Pulse"));
    }

    if (m_actionPause) {
        m_actionPause->setText(m_controller->recordingPaused() ? QStringLiteral("Resume Recording") : QStringLiteral("Pause Recording"));
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
