#include "csfatekcollector.h"
#include <QString>
CSFatekCollector::CSFatekCollector(QObject *parent) : QObject(parent)
{
    m_socket = new QTcpSocket(this);
    connect(m_socket, SIGNAL(connected()), this, SLOT(connected()));
    connect(m_socket, SIGNAL(disconnected()), this, SLOT(disconnected()));
    connect(m_socket, SIGNAL(readyRead()), this, SLOT(readyRead()));
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(error(QAbstractSocket::SocketError)));
    m_logFormat = "FatekCollector -> %1";
}

CSFatekCollector::~CSFatekCollector()
{
}

void CSFatekCollector::connected()
{
    emit newLog(m_logFormat.arg("Соединение установлено"));
    m_nextStationIndexForRequesting = 0;
    m_echoRequested = false;
    m_sendRequested = false;
    nextRequest();
}

void CSFatekCollector::disconnected()
{
    emit newLog(m_logFormat.arg("Соединение было разорвано"));
}

void CSFatekCollector::error(QAbstractSocket::SocketError)
{
    emit newLog(m_logFormat.arg(QString("ошибка соединения %1").arg(m_socket->errorString())));
}

QString CSFatekCollector::makeDumpLog(QByteArray &byteArray)
{
    QDataStream dump(&byteArray, QIODevice::ReadOnly);
    QString textDump;
    for(int d = 0; d<byteArray.size(); d++){
        quint8 dumpByte;
        dump >> dumpByte;
        textDump += QString::number(dumpByte, 16) + " ";
    }
    return textDump;
}

void CSFatekCollector::readyRead()
{
    QByteArray responce = m_socket->readAll();
    if (m_sendRequested){
        {//Запись дампа

            QString textDump = m_stationString + QString::number(m_forSending.first().station) + ": ";
            textDump += makeDumpLog(responce);
            emit newDumpLog(textDump);
        }
        QByteArray rightResponce;
        rightResponce.append(0x02); // STX
        int stationId = m_forSending.first().station;
        QString s_stationID = QString::number(stationId+256, 16).toUpper();
        rightResponce.append(s_stationID[1].toLatin1());
        rightResponce.append(s_stationID[2].toLatin1());
        rightResponce.append("470");
        addHash(rightResponce);
        rightResponce.append(0x03); // ETX
        if (rightResponce == responce){
            emit sended(m_forSending.first().sendingId, true);
        } else {
            emit sended(m_forSending.first().sendingId, false);
            emit newLog(m_logFormat.arg("Ошибка записи регистра"));
        }
        m_forSending.removeFirst();
        m_sendRequested=false;
        nextRequest();
        return;
    }
    if (m_echoRequested){
        {//Запись дампа
            QString textDump = m_stationString + QString::number(m_stationIds[m_nextStationIndexForRequesting]) + ": ";
            textDump += makeDumpLog(responce);
            emit newDumpLog(textDump);
        }
        QByteArray echoResponce = responce;
        bool isCorrect = checkEchoResponce(echoResponce);
        if (isCorrect){
            nextRequest();
        } else {
            emit newLog(m_logFormat.arg("Эхо ответ не корректен"));
            m_echoRequested = false;
            m_nextStationIndexForRequesting++;
            nextRequest();
        }
    } else {
        if (m_nextStationIndexForRequesting == -1){
            //Запись дампа
            QString textDump = "Неожидаемый ответ: ";
            textDump += makeDumpLog(responce);
            emit newDumpLog(textDump);
            return;
        }
        {//Запись дампа
            QString textDump = m_stationString + QString::number(m_stationIds[m_nextStationIndexForRequesting]) + ": ";
            textDump += makeDumpLog(responce);
            emit newDumpLog(textDump);
        }
        if (responce.size() < 6){
            emit newLog(m_logFormat.arg("неполный пакет"));
            m_nextStationIndexForRequesting++;
            nextRequest();
            return;
        }
        int offset = 0;
        if (0x02 != responce[offset+0]){
            emit newLog(m_logFormat.arg("неверный код STX"));
            m_nextStationIndexForRequesting++;
            nextRequest();
            return;
        }
        offset = 1;
        char s_station[3];
        s_station[0] = responce[offset+0];
        s_station[1] = responce[offset+1];
        s_station[2] = 0;
        int station = QString(s_station).toInt(0, 16);
        if (station != m_stationIds[m_nextStationIndexForRequesting]){
            emit newLog(m_logFormat.arg("неверный номер станции"));
            m_nextStationIndexForRequesting++;
            nextRequest();
            return;
        }
        offset = 3;
        char s_command[3];
        s_command[0] = responce[offset+0];
        s_command[1] = responce[offset+1];
        s_command[2] = 0;
        int command = QString(s_command).toInt(0, 16);
        if (command != 0x46){
            emit newLog(m_logFormat.arg("неверная команда"));
            m_nextStationIndexForRequesting++;
            nextRequest();
            return;
        }
        offset = 5;
        char s_code[2];
        s_code[0] = responce[offset+0];
        s_code[1] = 0;
        int code = QString(s_code).toInt(0, 16);
        if (code != 0x0){
            emit newLog(m_logFormat.arg(QString("код ошибки %1").arg(code)));
            m_nextStationIndexForRequesting++;
            nextRequest();
            return;
        }
        if (responce.size() != 9+m_registersNumber*4){
            emit newLog(m_logFormat.arg("неверный размер пакета"));
            m_nextStationIndexForRequesting++;
            nextRequest();
            return;
        }
        CSRegistersArray registerArray;
        registerArray.startRegister = m_firstRegister;
        registerArray.stationAddress = m_stationIds[m_nextStationIndexForRequesting];
        offset = 6;
        for(int i=0; i<m_registersNumber; i++){
            char s_value[5];
            s_value[0] = responce[offset+0];
            s_value[1] = responce[offset+1];
            s_value[2] = responce[offset+2];
            s_value[3] = responce[offset+3];
            s_value[4] = 0;
            int value = QString(s_value).toInt(0, 16);
            registerArray.values.append(value);
            offset += 4;
        }
        char s_hash[3];
        s_hash[0] = responce[offset+0];
        s_hash[1] = responce[offset+1];
        s_hash[2] = 0;
        int hash = QString(s_hash).toInt(0, 16);
        if (hash != calcHash(responce, 6+m_registersNumber*4)){
            emit newLog(m_logFormat.arg("неверная контрольная сумма"));
            m_nextStationIndexForRequesting++;
            nextRequest();
            return;
        }
        offset += 2;
        if (0x03 != responce[offset+0]){
            emit newLog(m_logFormat.arg("неверный код ETX"));
            m_nextStationIndexForRequesting++;
            nextRequest();
            return;
        }
        emit newRegistersArray(registerArray);
        m_nextStationIndexForRequesting++;
        nextRequest();
    }
}

bool CSFatekCollector::checkEchoResponce(QByteArray &echoResponce)
{
    QByteArray echoRequest = makeEchoRequest(m_stationIds[m_nextStationIndexForRequesting]);
    return echoRequest == echoResponce;
}

QByteArray CSFatekCollector::makeEchoRequest(int stationId)
{
    QByteArray echoRequest;
    echoRequest.append(0x02); // STX
    QString s_stationID = QString::number(stationId+256, 16).toUpper();
    echoRequest.append(s_stationID[1].toLatin1());
    echoRequest.append(s_stationID[2].toLatin1());
    echoRequest.append("4EABCDEFG"); // Эхо запрос
    addHash(echoRequest);
    echoRequest.append(0x03); // ETX
    return echoRequest;
}

void CSFatekCollector::sendEcho()
{
    QByteArray echoRequest = makeEchoRequest(m_stationIds[m_nextStationIndexForRequesting]);
    m_socket->write(echoRequest);
    emit newDumpLog(makeDumpLog(echoRequest).prepend(m_stationString+QString::number(m_stationIds[m_nextStationIndexForRequesting])+" >> "));
}

void CSFatekCollector::sendRequest()
{
    QByteArray request;
    request.append(0x02); // STX
    int stationId = m_stationIds[m_nextStationIndexForRequesting];
    QString s_stationID = QString::number(stationId+256, 16).toUpper();
    request.append(s_stationID[1].toLatin1());
    request.append(s_stationID[2].toLatin1());
    request.append("46"); // Чтение регистров
    QString s_registersNumber = QString::number(m_registersNumber+256,16).toUpper();
    request.append(s_registersNumber[1].toLatin1());
    request.append(s_registersNumber[2].toLatin1());
    request.append("R");
    QString s_firstRegister = QString::number(m_firstRegister+0x100000,16).toUpper();
    request.append(s_firstRegister[1].toLatin1());
    request.append(s_firstRegister[2].toLatin1());
    request.append(s_firstRegister[3].toLatin1());
    request.append(s_firstRegister[4].toLatin1());
    request.append(s_firstRegister[5].toLatin1());
    addHash(request);
    request.append(0x03); // ETX
    m_socket->write(request);
    emit newDumpLog(makeDumpLog(request).prepend(m_stationString+QString::number(m_stationIds[m_nextStationIndexForRequesting])+">> "));
}

void CSFatekCollector::send()
{
    QByteArray request;
    request.append(0x02); // STX
    int stationId = m_forSending.first().station;
    QString s_stationID = QString::number(stationId+256, 16).toUpper();
    request.append(s_stationID[1].toLatin1());
    request.append(s_stationID[2].toLatin1());
    request.append("4701R"); // Запись регистра
    QString s_register = QString::number(m_forSending.first().address+100000,10).toUpper();
    request.append(s_register[1].toLatin1());
    request.append(s_register[2].toLatin1());
    request.append(s_register[3].toLatin1());
    request.append(s_register[4].toLatin1());
    request.append(s_register[5].toLatin1());
    QString s_value = QString::number(m_forSending.first().value+0x10000,16).toUpper();
    request.append(s_value[1].toLatin1());
    request.append(s_value[2].toLatin1());
    request.append(s_value[3].toLatin1());
    request.append(s_value[4].toLatin1());
    addHash(request);
    request.append(0x03); // ETX
    m_socket->write(request);
  // emit newDumpLog(makeDumpLog(request).prepend(m_stationString+QString::number(m_stationIds[m_nextStationIndexForRequesting])+">> "));
}

void CSFatekCollector::nextRequest()
{
    // Отправка значений регистров
    if (m_socket->state() != QTcpSocket::ConnectedState){
        foreach(ForSending forSending, m_forSending){
          emit sended(forSending.sendingId, false);
        }
        m_forSending.clear();
        return;
    }
    if (m_forSending.size() > 0){
        send();
        m_sendRequested = true;
        return;
    }

    // Запрос на чтение регистров
    if (m_nextStationIndexForRequesting >= m_stationIds.size() ||
            m_nextStationIndexForRequesting == -1){
        m_nextStationIndexForRequesting = -1;
        return;
    }
    if (!m_echoRequested){
        sendEcho();
        m_echoRequested = true;
        return;
    } else {
        sendRequest();
        m_echoRequested = false;
    }
}

quint32 CSFatekCollector::sendRegister(quint8 station, quint16 address, quint16 value)
{
    static quint32 id = 0;
    ForSending forSending;
    forSending.address = address;
    forSending.station = station;
    forSending.value = value;
    forSending.sendingId = id;
    m_forSending.append(forSending);
    id++;
    if (m_nextStationIndexForRequesting == -1){
        nextRequest();
    }
    return id-1;
}

quint8 CSFatekCollector::calcHash(QByteArray &byteArray, int n)
{
    if (n < 0 || n > byteArray.size()){
        n = byteArray.size();
    }
    quint8 hash = 0;
    for(int i=0; i<n; i++){
        char c = byteArray[i];
        quint8 *pc = (quint8*)&c;
        hash += *pc;
    }
    return hash;
}

void CSFatekCollector::addHash(QByteArray &byteArray)
{
    quint8 hash = calcHash(byteArray);
    int res = hash;
    QString s_res = QString::number(res+256,16).toUpper();
    byteArray.append(s_res[1].toLatin1());
    byteArray.append(s_res[2].toLatin1());
}

void CSFatekCollector::timerEvent(QTimerEvent *)
{
    this->reconnect();
}

void CSFatekCollector::reconnect()
{
    if (m_socket->state() == QTcpSocket::ConnectedState)
        m_socket->disconnectFromHost();
    m_socket->connectToHost(m_ipAddress, m_port);
}

void CSFatekCollector::startRequesting()
{
    QString mes = QString("попытка соединения %1:%2").arg(m_ipAddress, QString::number(m_port));
    emit newLog(m_logFormat.arg(mes));
    m_socket->connectToHost(m_ipAddress, m_port);
    m_timerId = startTimer(m_requestPeriod);
}
