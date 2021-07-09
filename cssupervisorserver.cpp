#include "cssupervisorserver.h"

CSSupervisorServer::CSSupervisorServer(const CSSettings *settings, QString ipAddress, quint16 port, QObject *parent) : QObject(parent)
{
    m_settings = settings;
    server = new QTcpServer(this);
    connect(server, SIGNAL(newConnection()), SLOT(newConnection()));
    m_listening = server->listen(QHostAddress(ipAddress), port);
}

CSSupervisorServer::~CSSupervisorServer()
{

}

QString CSSupervisorServer::userBySocket(QTcpSocket *socket)
{
    if (m_socketUserHash.contains(socket))
        return m_socketUserHash[socket];
    else
        return "";
}

void CSSupervisorServer::newConnection()
{
    QTcpServer* server = static_cast<QTcpServer*>(sender());
    while (server->hasPendingConnections())
    {
        QTcpSocket *socket = server->nextPendingConnection();
        connect(socket, SIGNAL(readyRead()), SLOT(readyRead()));
        connect(socket, SIGNAL(disconnected()), SLOT(disconnected()));
        m_socketsSet.insert(socket);
    }
    emit newLog("Supervisor -> New supervisor was connected");
}

void CSSupervisorServer::sendToSocket(QTcpSocket* socket, const QString &line)
{
    if (m_socketsSet.contains(socket)){
        socket->write((line+"\n").toUtf8());
    }
}


void CSSupervisorServer::disconnected()
{
    QTcpSocket *socket = static_cast<QTcpSocket*>(sender());
    emit socketDisconnected(socket);
    m_socketsSet.remove(socket);
    if (m_socketUserHash.contains(socket)){
        emit newLog("Supervisor -> Supervisor " + m_socketUserHash[socket] + " was disconnected");
        emit userDisconnected(m_socketUserHash[socket]);
        m_socketUserHash.remove(socket);
    } else {
        emit newLog("Supervisor -> Unknown supervisor was disconnected");
    }
    socket->deleteLater();
}

void CSSupervisorServer::readyRead()
{
    QTcpSocket *socket = static_cast<QTcpSocket*>(sender());
    while (socket->canReadLine())
    {
        QString line = QString::fromUtf8(socket->readLine(10000));
        line.replace("\n","");
        QString command = line.section("|",0,0);
        if (command == "login"){
            QString userName = line.section("|",1,1);
            QString passwd = line.section("|",2,2);
            if (m_settings->m_userNameNickHash.contains(userName)){
                QString setingsUserNick = m_settings->m_userNameNickHash[userName];
                if (m_settings->value(setingsUserNick + "/password") == passwd){
                    socket->write(QString("done\n").toUtf8());
                    m_socketUserHash[socket] = userName;
                    emit newLog("Supervisor -> Supervisor " + userName + " was logined successful");
                    emit userLogined(userName);
                    continue;
                } else {
                    socket->write(QString("err\n").toUtf8());
                    //socket->disconnectFromHost();
                    //m_socketUserHash.insert(socket, userName);
                    return;
                }
            } else {
                socket->write(QString("err\n").toUtf8());
                //socket->disconnectFromHost();
                //m_socketUserHash.insert(socket, userName);
                return;
            }
        }
        if (this->m_socketUserHash.contains(socket) && socket->state() == QAbstractSocket::ConnectedState){
            if (command != "wantedRegisters")
                emit newLog(m_socketUserHash[socket]+": "+line);
            emit newCommand(line, socket);
        }
    }
}
