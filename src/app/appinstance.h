#pragma once
#include <QObject>
#include <QStringList>

namespace LoofiPulse {

// Keeps one interactive application window per user session. A second launch
// (widget, notification action, desktop file or CLI) forwards its arguments to
// the running instance over the session bus and exits.
class AppInstance : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.github.loofi.hardwarepulse.App1")
public:
    enum class Claim { Primary, Forwarded, Standalone };
    static constexpr const char *ServiceName = "com.github.loofi.hardwarepulse.App";
    static constexpr const char *ObjectPath = "/com/github/loofi/hardwarepulse/App";

    explicit AppInstance(QObject *parent = nullptr);
    // Primary: this process owns the name. Forwarded: an existing instance accepted
    // the arguments. Standalone: no usable session bus; run without uniqueness.
    Claim claim(const QStringList &arguments, const QString &workingDirectory);

public Q_SLOTS:
    Q_SCRIPTABLE void Activate(const QStringList &arguments, const QString &workingDirectory, const QString &activationToken);

Q_SIGNALS:
    void activateRequested(const QStringList &arguments, const QString &workingDirectory, const QString &activationToken);
};

}
