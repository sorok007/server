#ifndef CSFATEKCOLLECTOR_H
#define CSFATEKCOLLECTOR_H

#include <QtNetwork>
#include <QObject>
#include "cstypes.h"

struct ForSending{
    quint32 sendingId;
    quint8 station;
    quint16 address;
    quint16 value;
};

class CSFatekCollector : public QObject
{
    Q_OBJECT
public:
    explicit CSFatekCollector(QObject *parent = 0);
    ~CSFatekCollector();
    void setIpAddress(const QString &ipAddress) { m_ipAddress = ipAddress; }
    void setPort(quint16 port) { m_port = port; }
    void addStationId(quint8 stationId) { m_stationIds.append(stationId); }
    void setFirstRegister(int firstRegister) { m_firstRegister = firstRegister; }
    void setRegistersNumber(int registersNumber) { m_registersNumber = registersNumber; }
    void setRequestPeriod(int requestPeriod) { m_requestPeriod = requestPeriod; }
    void startRequesting();
    quint32 sendRegister(quint8 station, quint16 address, quint16 value);
    void setLogFormat(const QString &format) { m_logFormat = format; }
    void setStationString(const QString &stationString) { m_stationString = stationString; }
    void reconnect();

protected:
    void timerEvent(QTimerEvent *);

private:
    QString m_stationString;
    QString m_logFormat;
    int m_timerId;
    int m_requestPeriod;
    QString m_ipAddress;
    quint16 m_port;
    QVector<quint8> m_stationIds;
    int m_nextStationIndexForRequesting;
    bool m_echoRequested;
    QList<ForSending> m_forSending;
    bool m_sendRequested;
    int m_firstRegister;
    int m_registersNumber;
    QTcpSocket* m_socket;
    void nextRequest();
    void sendRequest();
    void sendEcho();
    void send();
    QString makeDumpLog(QByteArray &byteArray);
    bool checkEchoResponce(QByteArray &echoResponce);
    QByteArray makeEchoRequest(int stationId);
    void addHash(QByteArray &byteArray);
    quint8 calcHash(QByteArray &byteArray, int n = -1);

signals:
    void newRegistersArray(const CSRegistersArray &registersArray);
    void newDumpLog(const QString &log);
    void newLog(const QString &log);
    void sended(quint32 sendingId, bool ok);

private slots:
    void connected();
    void disconnected();
    void readyRead();
    void error(QAbstractSocket::SocketError);
};

#endif // CSFATEKCOLLECTOR_H
