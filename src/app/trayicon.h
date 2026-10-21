#pragma once

#include <QObject>
#include <QString>

class QSystemTrayIcon;
class QMenu;
class QAction;
class QWindow;

namespace LoofiPulse {
class PulseTelemetryController;

class TrayIcon : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(bool minimizeToTrayOnClose READ minimizeToTrayOnClose WRITE setMinimizeToTrayOnClose NOTIFY minimizeToTrayOnCloseChanged)

public:
    explicit TrayIcon(QObject *parent = nullptr);
    ~TrayIcon() override;

    bool isAvailable() const;
    bool isVisible() const;
    void setVisible(bool visible);

    bool minimizeToTrayOnClose() const { return m_minimizeToTrayOnClose; }
    void setMinimizeToTrayOnClose(bool enable);

    void init(PulseTelemetryController *controller, QWindow *mainWindow);

    Q_INVOKABLE void showInformation(const QString &title, const QString &message, int timeoutMs = 5000);

Q_SIGNALS:
    void quitRequested();
    void availableChanged();
    void visibleChanged();
    void minimizeToTrayOnCloseChanged();

private Q_SLOTS:
    void onTelemetryUpdated();
    void onTrayActivated(int reason);
    void toggleWindowVisibility();

private:
    void updateTooltipAndIcon();
    void updateMenu();

    PulseTelemetryController *m_controller = nullptr;
    QWindow *m_mainWindow = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_actionToggleWindow = nullptr;
    QAction *m_actionMarkProblem = nullptr;
    QAction *m_actionPause = nullptr;
    QAction *m_actionSnooze = nullptr;
    QAction *m_actionQuit = nullptr;

    bool m_visible = true;
    bool m_minimizeToTrayOnClose = true;
};

} // namespace LoofiPulse
