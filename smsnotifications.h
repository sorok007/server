#ifndef SMSNOTIFICATIONS_H
#define SMSNOTIFICATIONS_H
#include <QObject>
#include <QString>
#include <QMap>
#include <QStringList>

class Server;

class NotificationConfig {
    bool m_enabled;
    bool m_triggered;
    QString m_parameterName;
    QStringList m_phonesList;
    float m_threshold;
    QString m_message;
public:
    NotificationConfig();

    bool enabled() {return m_enabled;}

    bool triggered() {return m_triggered;}
    void setTriggered(bool triggered) {m_triggered = triggered;}

    const QString& parameterName() {return m_parameterName;}

    const QStringList& phonesList() {return m_phonesList;}

    float threshold() {return m_threshold;}

    QString message() {return m_message;}

    bool init(const QJsonObject &jsonObj);
};

class SMSNotificationsProcessor: QObject
{
    Q_OBJECT

    bool m_initialised;
    QMap<QString, QString> m_phones;
    QMap<QString, NotificationConfig> m_alerts;
    bool init();
public:
    SMSNotificationsProcessor();

public slots:
    void processNewValue(const QString &parameterName, const QString &value, Server* smsSender);
};

#endif // SMSNOTIFICATIONS_H
