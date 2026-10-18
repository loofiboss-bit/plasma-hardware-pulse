#pragma once
#include <QDBusContext>
#include <QObject>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <functional>
#include "pulsetelemetrycontroller.h"

namespace LoofiPulse {
class CollectorService : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.github.loofi.hardwarepulse.Collector1")
public:
    explicit CollectorService(QObject *parent = nullptr);
    ~CollectorService() override;
public Q_SLOTS:
    QByteArray Subscribe(const QString &clientId, int interval, int retention, bool notifications);
    QByteArray Unsubscribe(const QString &clientId);
    QByteArray History(qulonglong afterId, int limit);
    QByteArray Incident(const QString &id);
    QByteArray MarkProblem();
    QByteArray OpenIncident(const QString &id);
    QByteArray OpenApplication(const QString &page, const QString &section);
    QByteArray ClearHistory();
    QByteArray SetBackground(bool enabled);
    QByteArray SetStartAtLogin(bool enabled);
    QByteArray Snooze();
    QByteArray StopMonitoring();
    QByteArray GetPreferences();
    QByteArray SetPreferences(const QByteArray &json);
Q_SIGNALS:
    void FrameReady(const QByteArray &packet);
    void PreferencesChanged(const QByteArray &packet);
private Q_SLOTS:
    void ownerChanged(const QString &name, const QString &previous, const QString &owner);
    void onPrepareForSleep(bool goingToSleep);
private:
    struct Client { QString owner; int interval = 1000; int retention = 300; bool notifications = true; };
    QByteArray dispatch(std::function<QByteArray(PulseTelemetryController *)> operation);
    void updateRequests();
    void publish(const QByteArray &packet);
    void notify(const QVariantMap &packet);
    bool backgroundActive() const;
    bool m_loginMonitoring = false;
    QThread m_thread;
    QObject *m_anchor = nullptr;
    PulseTelemetryController *m_engine = nullptr;
    QHash<QString, Client> m_clients;
    QTimer m_idle;
    QTimer m_busHealth;
    QSettings m_settings{QStringLiteral("LoofiPulse"), QStringLiteral("Collector")};
    QByteArray m_latest;
    QHash<QString, int> m_notified;
    qint64 m_snoozeUntil = 0;
};
}
