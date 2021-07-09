#ifndef CSMODBUSSERVER_H
#define CSMODBUSSERVER_H

#include <QtNetwork>
#include <QObject>
#include "cstypes.h"

class CSModbusServer : public QObject
{
    Q_OBJECT
public:
    explicit CSModbusServer(QString ipAddress, quint16 port, QObject *parent = 0);
    ~CSModbusServer();
    bool isSuccess(){return m_listening;}
    //Каждому сокету свой буффер
    QHash<QTcpSocket*, QByteArray*> m_socketBufferHash;
    //Установка времени ожидания зависших станций до их отключения
    void setTimeOut(int timeout){ m_timeout = timeout; }
    //Установка периода проверки зависших станций
    //По уолчанию проверка таймаута каждые 10 секунд
    void setTimeOutCheckPeriod(int msec);
    bool isListining() { return m_listening; }
    const QTcpServer* getServer() { return m_server; }
    void setOutLogFormats(const QString &format) { m_outLogFormat = format; }
    void setInLogFormats(const QString &format) { m_inLogFormat = format; }

private:
    //время ожидания зависших станций до их отключения
    int m_timeout;
    //Время последней передачи по каждому сокету для отключения зависших станций
    QHash<QTcpSocket*, QDateTime> m_lastConnectionsHash;
    //Флаг успешного старта modbus сервера
    bool m_listening;
    //Идентификатор таймера
    int timerId;
    QTcpServer *m_server;
    QString m_outLogFormat;
    QString m_inLogFormat;
    QString makeDumpLog(QByteArray& byteArray, int n = -1);

signals:
    void newRegistersArray(const CSRegistersArray &registersArray);
    void newDumpLog(const QString &log);
    void newLog(const QString &log);
    void newMsg(const QString &msg);

private slots:
    //Новое входящее соединение
    void newConnection();
    //Разрыв соединения
    void disconnected();
    //Новые входящие данные
    void readyRead();

protected:
    void timerEvent(QTimerEvent * event);
};

#endif // CSMODBUSSERVER_H
