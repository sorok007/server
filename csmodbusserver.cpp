#include "csmodbusserver.h"
#include <QMessageBox>

CSModbusServer::CSModbusServer(QString ipAddress, quint16 port, QObject *parent) : QObject(parent)
{
    m_timeout = 60*10; //По умолчанию таймаут входящего соединения 10 минут
    m_server = new QTcpServer(this);
    connect(m_server, SIGNAL(newConnection()), SLOT(newConnection()));
    m_listening = m_server->listen(QHostAddress(ipAddress), port);
    timerId = 0; // Таймер не установлен
    setTimeOutCheckPeriod(10000); // По уолчанию проверка таймаута каждые 10 секунд
    // %1 - номер станции; %2 - dump
    m_outLogFormat = "modbus >> %1 %2";
    m_inLogFormat = "modbus %1 %2";
}

void CSModbusServer::setTimeOutCheckPeriod(int msec){
    if (timerId)
        killTimer(timerId);
    timerId = startTimer(msec);
}

CSModbusServer::~CSModbusServer()
{
    foreach(QByteArray* buff, m_socketBufferHash){
        delete buff;
    }
}

void CSModbusServer::newConnection()
{
    QTcpServer* server = static_cast<QTcpServer*>(sender());
    while (server->hasPendingConnections())
    {
        QTcpSocket *socket = server->nextPendingConnection();
        connect(socket, SIGNAL(readyRead()), SLOT(readyRead()));
        connect(socket, SIGNAL(disconnected()), SLOT(disconnected()));
        QByteArray *buffer = new QByteArray();
        m_socketBufferHash.insert(socket, buffer);
        m_lastConnectionsHash[socket] = QDateTime::currentDateTime();
        emit newLog("Modbus -> Входящее соединение, всего: " +
                    QString::number(m_socketBufferHash.size()));
    }
}

void CSModbusServer::disconnected()
{
    QTcpSocket *socket = static_cast<QTcpSocket*>(sender());
    QByteArray *buffer = m_socketBufferHash.value(socket);
    delete buffer;
    m_socketBufferHash.remove(socket);
    m_lastConnectionsHash.remove(socket);
    socket->deleteLater();
    emit newLog("Modbus -> Соединение разорвано, активных: " +
                            QString::number(m_socketBufferHash.size()));
}

QString CSModbusServer::makeDumpLog(QByteArray &byteArray, int n)
{
    if (n < 0 || n > byteArray.size())
        n = byteArray.size();
    QString textDump;
    QDataStream dump(&byteArray, QIODevice::ReadOnly);
    for(int d = 0; d<n; d++){
        quint8 dumpByte;
        dump >> dumpByte;
        textDump += QString::number(dumpByte, 16) + " ";
    }
    return textDump;
}

void CSModbusServer::readyRead()
{
    QTcpSocket *socket = static_cast<QTcpSocket*>(sender());
    QByteArray *buffer = m_socketBufferHash.value(socket);
    //Флаг продолжения чтения возможно пришедшего следующего пакета
    bool cont = true;
    m_lastConnectionsHash[socket] = QDateTime::currentDateTime();
    while (socket->bytesAvailable() > 0 || cont){
        cont = false;
        buffer->append(socket->readAll());
        if (buffer->size() >= 12){

            QString bufStr("");
            for (int i=0;i<buffer->size();++i) {
                bufStr += QString(" %1").arg((ushort)(buffer->at(i)), 2, 16, QChar('0'));
            }
            emit newMsg(bufStr);

            QDataStream data(buffer, QIODevice::ReadOnly);
            Word16 messId, tcpCode, size;
            data >> messId.wh >> messId.wl;
            data >> tcpCode.wh >> tcpCode.wl;
            data >> size.wh >> size.wl;
            quint8 address;
            data >> address;
            quint8 command;
            data >> command;
            if (buffer->size() - 6 >= size.w){
                if (command == 6){
                    Word16 reg, val;
                    data >> reg.wh >> reg.wl >> val.wh >> val.wl;
                    CSRegistersArray registersArray;
                    registersArray.stationAddress = address;
                    registersArray.startRegister = reg.w;
                    registersArray.values.append(val.w);
                    emit newRegistersArray(registersArray);
                    {//Запись дампа
                        QString textDump = makeDumpLog(*buffer, 12);
                        emit newDumpLog(m_inLogFormat.arg("station"+QString::number(address)+" (command 6):", textDump));
                    }
                    QByteArray outBuffer = buffer->left(12);
                    socket->write(outBuffer);
                    {//Запись дампа
                        QString textDump = makeDumpLog(outBuffer);
                        emit newDumpLog(m_outLogFormat.arg("station"+QString::number(address)+":", textDump));
                    }
                    buffer->remove(0, 12);
                    cont = true;
                    continue;
                }
                if (command == 16){
                    Word16 freg, regs;
                    quint8 bytes;
                    data >> freg.wh >> freg.wl >> regs.wh >> regs.wl;
                    //Количество байт в списке значений регистров не проверяется
                    //Проверяется только общий размер пакета без заголовка (size)
                    data >> bytes;
                    CSRegistersArray registerArray;
                    registerArray.stationAddress = address;
                    registerArray.startRegister = freg.w;
                    registerArray.values.reserve(regs.w);
                    for(int i = freg.w; i < freg.w+regs.w; i++){
                        Word16 val;
                        data >> val.wh >> val.wl;
                        registerArray.values.append(val.w);
                    }
                    emit newRegistersArray(registerArray);
                    {//Запись дампа
                        QString textDump = makeDumpLog(*buffer, 6+size.w);
                        emit newDumpLog(m_inLogFormat.arg("station"+QString::number(address)+" (command 16):", textDump));
                    }
                    QByteArray outBuffer = buffer->left(12);
                    socket->write(outBuffer);
                    {//Запись дампа
                        QString textDump = makeDumpLog(outBuffer);
                        emit newDumpLog(m_outLogFormat.arg("station"+QString::number(address)+":", textDump));
                    }
                    buffer->remove(0, 6+size.w);
                    cont = true;
                    continue;
                }
                {//Запись дампа
                    QString textDump = makeDumpLog(*buffer, 6+size.w);
                    emit newDumpLog(m_inLogFormat.arg("(unknown command):", textDump));
                }
                QByteArray outBuffer = buffer->left(9);
                //outBuffer.resize(9);
                outBuffer[5] = (quint8)3;
                outBuffer[7] = (quint8)(128+6);
                outBuffer[8] = (quint8)1;
                socket->write(outBuffer);
                {//Запись дампа
                    QString textDump = makeDumpLog(outBuffer);
                    emit newDumpLog(m_outLogFormat.arg(":", textDump));
                }
                buffer->remove(0, 6+size.w);
                continue;
            }///if (size.w <= buffer->size() - 6)
        }///if (buffer->size() >= 8)
    }///while (socket->bytesAvailable() > 0)
}

void CSModbusServer::timerEvent(QTimerEvent*)
{
    //Завершение соединений с молчащими станциями
    QList<QTcpSocket*> silentStations;
    for(QHash<QTcpSocket*, QDateTime>::iterator i = m_lastConnectionsHash.begin(); i != m_lastConnectionsHash.end(); i++){
        if (i.value().addSecs(m_timeout) < QDateTime::currentDateTime()){
            silentStations.append(i.key());
        }
    }
    foreach(QTcpSocket* socket, silentStations){
        socket->disconnectFromHost();
    }
}
