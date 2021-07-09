#ifndef CSMODBUSCOLLECTOR_H
#define CSMODBUSCOLLECTOR_H

#include <QObject>
#include <QtNetwork>

class CSRegistersArray;

class CSModbusCollector : public QObject
{
    Q_OBJECT
public:
    explicit CSModbusCollector(QObject *parent = 0);
    ~CSModbusCollector();
    void setIpAddress(const QString &ipAddress) { m_ipAddress = ipAddress; }
    void setPort(quint16 port) { m_port = port; }
    void setStationId(quint8 stationId) { m_stationId = stationId; }
    void setFirstRegister(int firstRegister) { m_firstRegister = firstRegister; }
    void setRegistersCount(int registersNumber) { m_registersCount = registersNumber; }
    void setRequestPeriod(int requestPeriod) { m_requestPeriod = requestPeriod; }

    void startRequesting();
    //quint32 sendRegister(quint8 station, quint16 address, quint16 value);
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
    quint8 m_stationId;
    int m_nextStationIndexForRequesting;
    bool m_echoRequested;
    //QList<ForSending> m_forSending;
    bool m_sendRequested;
    int m_firstRegister;
    int m_registersCount;
    QTcpSocket* m_socket;
    void nextRequest();
    void sendReadRequest();
    QString makeDumpLog(QByteArray &byteArray);
    quint16 calcCrc16(const QByteArray &byteArray);

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

#endif // CSMODBUSCOLLECTOR_H
