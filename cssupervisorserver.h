#ifndef CSSUPERVISORSERVER_H
#define CSSUPERVISORSERVER_H

#include <QObject>
#include <QtNetwork>
#include <QSet>
#include <QHash>
#include "cssettings.h"

class CSSupervisorServer : public QObject
{
    Q_OBJECT
public:
    explicit CSSupervisorServer(const CSSettings *settings, QString ipAddress, quint16 port, QObject *parent);
    ~CSSupervisorServer();
    bool isListining() { return m_listening; }
    const QTcpServer* getServer() { return server; }
    void sendToSocket(QTcpSocket* socket, const QString& line);
    QString userBySocket(QTcpSocket* socket);

private:
    bool m_listening;
    QSet<QTcpSocket*> m_socketsSet;
    QHash<QTcpSocket*, QString> m_socketUserHash;
    const CSSettings *m_settings;
    QTcpServer *server;

signals:
    void newLog(const QString &log);
    void newCommand(const QString &line, QTcpSocket* socket);
    void socketDisconnected(QTcpSocket* socket);
    void userDisconnected(const QString& userName);
    void userLogined(const QString& userName);


private slots:
    void newConnection();
    void disconnected();
    void readyRead();
};

#endif // CSSUPERVISORSERVER_H
