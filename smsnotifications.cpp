#include "smsnotifications.h"
#include "server.h"
#include <QtCore>

NotificationConfig::NotificationConfig() : m_triggered(false)
{}

bool NotificationConfig::init(const QJsonObject &jsonObj)
{
    QJsonValue valEnabled = jsonObj.value("enabled");
    if (valEnabled != QJsonValue::Undefined && valEnabled.isBool()) {
        m_enabled = valEnabled.toBool();
    }

    QJsonValue valName = jsonObj.value("parameterName");
    if (valName != QJsonValue::Undefined && valName.isString()) {
        m_parameterName = valName.toString();
    } else return false;

    QJsonValue valThreshold = jsonObj.value("threshold");
    if (valThreshold != QJsonValue::Undefined && valThreshold.isDouble()) {
        m_threshold = valThreshold.toDouble();
    }

    QJsonValue valMessage = jsonObj.value("message");
    if (valMessage != QJsonValue::Undefined && valMessage.isString()) {
        m_message = valMessage.toString();
    } else return false;

    QJsonValue valPhones = jsonObj.value("phones");
    if (valMessage != QJsonValue::Undefined && valPhones.isArray()) {
        QJsonArray phones = valPhones.toArray();
        for (int i = 0; i<phones.count(); ++i) {
            QJsonValue valPhone = phones.at(i);
            if (valPhone != QJsonValue::Undefined && valPhone.isString()) {
                m_phonesList.push_back(valPhone.toString());
            }
        }
    } else return false;

    return true;
}


SMSNotificationsProcessor::SMSNotificationsProcessor() : m_initialised(false)
{
    init();
}

bool SMSNotificationsProcessor::init()
{
    QFile fileConfig("sms_notifications_config.json");

    if (!fileConfig.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        //strError = "невозможно открыть файл ";
        return false;
    }

    QByteArray fileContent = fileConfig.readAll();
    fileConfig.close();
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(fileContent, &error);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        QString es = error.errorString();
        return false;
    }

    QJsonObject docObj = jsonDoc.object();

    QJsonValue valPhones = docObj.value("phones");
    if (valPhones != QJsonValue::Undefined && valPhones.isObject()) {
        QJsonObject objPhones = valPhones.toObject();
        Q_FOREACH(QString key, objPhones.keys()) {
            QJsonValue valPhoneNumber = objPhones.value(key);
            if (valPhoneNumber.isString() && !valPhoneNumber.toString().isEmpty()) {
                m_phones.insert(key, valPhoneNumber.toString());
            }
        }
    }

    QJsonValue valNotifications = docObj.value("notifications");
    if (valNotifications != QJsonValue::Undefined && valNotifications.isArray()) {
        QJsonArray notifications = valNotifications.toArray();
        for (int i = 0; i<notifications.count(); ++i) {
            QJsonValue valNotification = notifications.at(i);
            if (valNotification != QJsonValue::Undefined && valNotification.isObject()) {
                NotificationConfig notification;
                if (notification.init(valNotification.toObject()))
                {
                    m_alerts.insert(notification.parameterName(), notification);
                } else return false;
            } else return false;
        }
    } else return false;

    m_initialised = true;
    return m_initialised;
}

void SMSNotificationsProcessor::processNewValue(const QString &parameterName, const QString &fullValue, Server* smsSender)
{
    if (!m_initialised)
        return;

    QString strDateTime = fullValue.section("|", 0, 0);
    QString value = fullValue.section("|", 1, 1);

    QRegExp vnsPumpRegExp("^vns[2-6].pump\\d{1}State$");
    QRegExp borePumpRegExp("^bore\\d{1,2}.pump1State$");
    QRegExp boreOpenRegExp("^bore\\d{1,2}.openState$");
    QRegExp boreFireRegExp("^bore\\d{1,2}.fireState$");
    QRegExp borePowerRegExp("^bore\\d{1,2}.powerState$");
    QRegExp boreTempRegExp("^bore\\d{1,2}.tempState$");

    bool sendSms = false;
    QStringList phonesList;
    QString message;

    if (borePumpRegExp.exactMatch(parameterName)) {
        QString boreNumStr = parameterName.section('.', 0, 0).mid(4);

        QString configKeyName = "bore.pumpState";
        if (m_alerts.contains(configKeyName)) {
            NotificationConfig& c = m_alerts[configKeyName];
            if (c.enabled()) {
                sendSms = true;
                message = c.message().arg(boreNumStr).arg(value == "3" ? "включен" : "выключен");
                Q_FOREACH(QString phoneKey, c.phonesList()) {
                    if (m_phones.contains(phoneKey)) {
                        phonesList.append(m_phones.value(phoneKey));
                    }
                }
            }
        }
    }

    if (boreOpenRegExp.exactMatch(parameterName) ||
        boreFireRegExp.exactMatch(parameterName) ||
        borePowerRegExp.exactMatch(parameterName) ||
        boreTempRegExp.exactMatch(parameterName)) {
        QString boreNumStr = parameterName.section('.', 0, 0).mid(4);

        QString configKeyName = "bore." + parameterName.section('.', 1, 1);
        if (m_alerts.contains(configKeyName)) {
            NotificationConfig& c = m_alerts[configKeyName];
            if (c.enabled() && (value!= "0")) {
                sendSms = true;
                message = c.message().arg(boreNumStr);
                Q_FOREACH(QString phoneKey, c.phonesList()) {
                    if (m_phones.contains(phoneKey)) {
                        phonesList.append(m_phones.value(phoneKey));
                    }
                }
            }
        }
    }

    if (parameterName == "_bs.openState" ||
        parameterName == "_bs.tempState" ||
        parameterName == "vns6.openState" ||
        parameterName == "vns2.errorPC" ||
        parameterName == "vns3.errorPC" ||
        parameterName == "vns4.errorPC" ||
        parameterName == "vns5.errorPC" ||
        parameterName == "vns6.errorPC") {
        if (m_alerts.contains(parameterName)) {
            NotificationConfig& c = m_alerts[parameterName];
            if (c.enabled()) {
                if (!c.triggered() && (value!= "0")) {
                    sendSms = true;
                    message = c.message();
                    Q_FOREACH(QString phoneKey, c.phonesList()) {
                        if (m_phones.contains(phoneKey)) {
                            phonesList.append(m_phones.value(phoneKey));
                        }
                    }
                    c.setTriggered(true);
                } else if (c.triggered() && value == "0") {
                    c.setTriggered(false);
                }
            }
        }
    }

    if (parameterName == "_bs.presValue") {
        QString minValueName = parameterName + "_min";
        QString maxValueName = parameterName + "_max";

        if (m_alerts.contains(minValueName)) {
            NotificationConfig& c = m_alerts[minValueName];
            if (c.enabled()) {
                float fvalue = value.toFloat();
                if (!c.triggered() && fvalue < c.threshold()) {
                    sendSms = true;
                    message = c.message().arg(value);
                    Q_FOREACH(QString phoneKey, c.phonesList()) {
                        if (m_phones.contains(phoneKey)) {
                            phonesList.append(m_phones.value(phoneKey));
                        }
                    }
                    c.setTriggered(true);
                } else if (c.triggered() && fvalue >= c.threshold()) {
                    c.setTriggered(false);
                }
            }
        }

        if (m_alerts.contains(maxValueName)) {
            NotificationConfig& c = m_alerts[maxValueName];
            if (c.enabled()) {
                float fvalue = value.toFloat();
                if (!c.triggered() && fvalue > c.threshold()) {
                    sendSms = true;
                    message = c.message().arg(value);
                    Q_FOREACH(QString phoneKey, c.phonesList()) {
                        if (m_phones.contains(phoneKey)) {
                            phonesList.append(m_phones.value(phoneKey));
                        }
                    }
                    c.setTriggered(true);
                } else if (c.triggered() && fvalue <= c.threshold()) {
                    c.setTriggered(false);
                }
            }
        }
    }

    if (parameterName.startsWith("vns1.freqValue")) {
        static float freqValue1 = 0.0;
        static float freqValue2 = 0.0;

        if (parameterName == "vns1.freqValue1") {
            freqValue1 = value.toFloat();
        }

        if (parameterName == "vns1.freqValue2") {
            freqValue2 = value.toFloat();
        }

        QString configKeyName = "vns1.freqValue";
        if (m_alerts.contains(configKeyName)) {
            NotificationConfig& c = m_alerts[configKeyName];
            if (c.enabled()) {
                if (!c.triggered() && (freqValue1 >= c.threshold() || freqValue2 >= c.threshold())) {
                    sendSms = true;
                    message = c.message().arg(value);
                    Q_FOREACH(QString phoneKey, c.phonesList()) {
                        if (m_phones.contains(phoneKey)) {
                            phonesList.append(m_phones.value(phoneKey));
                        }
                    }
                    c.setTriggered(true);
                } else if (c.triggered() && freqValue1 < c.threshold() && freqValue2 < c.threshold()) {
                    c.setTriggered(false);
                }
            }
        }

    }

    if (parameterName == "vns1.waterLevelValue" ||
        parameterName == "vns3.waterLevelValue") {
        if (m_alerts.contains(parameterName)) {
            NotificationConfig& c = m_alerts[parameterName];
            if (c.enabled()) {
                float fvalue = value.toFloat();
                if (!c.triggered() && qFuzzyCompare(fvalue, float(100.0))) {
                    sendSms = true;
                    message = c.message();
                    Q_FOREACH(QString phoneKey, c.phonesList()) {
                        if (m_phones.contains(phoneKey)) {
                            phonesList.append(m_phones.value(phoneKey));
                        }
                    }
                    c.setTriggered(true);
                } else if (c.triggered() && qFuzzyCompare(fvalue, float(100.0))) {
                    c.setTriggered(false);
                }
            }
        }
    }

    if (parameterName == "vns2.freqValue" ||
        parameterName == "vns3.freqValue" ||
        parameterName == "vns4.freqValue" ||
        parameterName == "vns5.freqValue" ||
        parameterName == "vns6.freqValue") {
        if (m_alerts.contains(parameterName)) {
            NotificationConfig& c = m_alerts[parameterName];
            if (c.enabled()) {
                float fvalue = value.toFloat();
                if (!c.triggered() && fvalue >= c.threshold()) {
                    sendSms = true;
                    message = c.message().arg(value);
                    Q_FOREACH(QString phoneKey, c.phonesList()) {
                        if (m_phones.contains(phoneKey)) {
                            phonesList.append(m_phones.value(phoneKey));
                        }
                    }
                    c.setTriggered(true);
                } else if (c.triggered() && fvalue < c.threshold()) {
                    c.setTriggered(false);
                }
            }
        }
    }

    if (parameterName == "vns2.presOutValue" ||
        parameterName == "vns3.presOutValue" ||
        parameterName == "vns4.presOutValue" ||
        parameterName == "vns5.presOutValue" ||
        parameterName == "vns6.presOutValue") {
        if (m_alerts.contains(parameterName)) {
            NotificationConfig& c = m_alerts[parameterName];
            if (c.enabled()) {
                float fvalue = value.toFloat();
                if (!c.triggered() && fvalue < c.threshold()) {
                    sendSms = true;
                    message = c.message().arg(value);
                    Q_FOREACH(QString phoneKey, c.phonesList()) {
                        if (m_phones.contains(phoneKey)) {
                            phonesList.append(m_phones.value(phoneKey));
                        }
                    }
                    c.setTriggered(true);
                } else if (c.triggered() && fvalue >= c.threshold()) {
                    c.setTriggered(false);
                }
            }
        }
    }

    if (vnsPumpRegExp.exactMatch(parameterName)) {
        QString pumpNumStr = parameterName.mid(9, 1);

        QString configKeyName = QString(parameterName).remove(9, 1);
        if (m_alerts.contains(configKeyName)) {
            NotificationConfig& c = m_alerts[configKeyName];
            if (c.enabled()) {
                sendSms = true;
                message = c.message().arg(pumpNumStr).arg(value == "1" ? "On" : "Off");
                Q_FOREACH(QString phoneKey, c.phonesList()) {
                    if (m_phones.contains(phoneKey)) {
                        phonesList.append(m_phones.value(phoneKey));
                    }
                }
            }
        }
    }

    if (sendSms && !message.isEmpty()) {
        message = strDateTime + ": " + message;
        Q_FOREACH(QString phone, phonesList) {
            smsSender->sendSMS(phone, message);
        }
    }
}
