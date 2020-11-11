#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QtNetwork>
#include <QStringList>
#include "mainwindow.h"
#include "cssettings.h"
#include "cssqlitestorage.h"
#include "cssupervisorserver.h"
#include "csfatekcollector.h"
#include "csmodbusserver.h"
#include "smsnotifications.h"

struct PumpStationDefinition;

class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(QObject *parent = 0);
    ~Server();
    bool sendSMS(const QString& number, const QString& text);
private:
    MainWindow m_mainWindow;
    QVector<PumpStationDefinition>* m_vnsDefinitions;
    CSSettings *m_settings;
    CSSQLiteStorage *m_sqlstorage;
    CSSupervisorServer *m_supervisorServer;
    QHash<QString, CSFatekCollector*> m_boreFatekCollectors;
    QHash<QString, CSFatekCollector*> m_koskFatekCollectors;
    QStringList m_boreIds;
    QStringList m_koskIds;
    QHash<QString, CSFatekCollector*> m_vnsFatekCollectors;
    CSModbusServer *m_modbusServer;
    QHash<QTcpSocket*, QSet<QString> > m_socketRegisters;
    QDateTime m_startingTime;
    QHash<QString, QString> m_stationNames;
    QHash<QString, QString> m_stateStrings;
    QSet<QString> namesWithIntervalsTechnomer;
    SMSNotificationsProcessor m_smsProcessor;
    void setStorage();
    void setSupervisorServer();
    void setModbusServer();
    void setBores();
    void setKosk();
    void setVNS(const PumpStationDefinition &def);
    void requestVNS(const QString& name);
    void sendReg(QTcpSocket *socket, const QString &fullName, const QString &fullValue);
    void setPressure(const QString &name, const QString &value);

signals:

public slots:
    void userLogined(const QString & userName);
    void userDisconnected(const QString & userName);
    void sendTestSMS();
    void newCommand(const QString &line, QTcpSocket* socket);
    void boreSwitchOn();
    void boreSwitchOff();
    void toBoreSended(quint32 id, bool ok);
    void newModbusRegisterArray(const CSRegistersArray &registerArray);
    void newBoresRegisterArray(const CSRegistersArray &registerArray);
    void newKosksRegisterArray(const CSRegistersArray &registerArray);
    void newVnsRegisterArray(const CSRegistersArray &registerArray);
    void socketDisconnected(QTcpSocket* socket);
    void newValue(const QString &fullName, const QString &fullValue);
    void dumpModbusMsg(const QString &msg);

protected:
    void timerEvent(QTimerEvent*);
};

#endif // SERVER_H
