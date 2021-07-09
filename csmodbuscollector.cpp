#include "csmodbuscollector.h"
#include "cstypes.h"

CSModbusCollector::CSModbusCollector(QObject *parent) : QObject(parent)
{
    m_socket = new QTcpSocket(this);
    connect(m_socket, SIGNAL(connected()), this, SLOT(connected()));
    connect(m_socket, SIGNAL(disconnected()), this, SLOT(disconnected()));
    connect(m_socket, SIGNAL(readyRead()), this, SLOT(readyRead()));
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(error(QAbstractSocket::SocketError)));
    m_logFormat = "VnsModbusCollector -> %1";
}

CSModbusCollector::~CSModbusCollector()
{
    m_socket->close();
    delete m_socket;
}


void CSModbusCollector::connected()
{
    emit newLog(m_logFormat.arg("Соединение установлено"));
    nextRequest();
}

void CSModbusCollector::disconnected()
{
    emit newLog(m_logFormat.arg("Соединение было разорвано"));
}

void CSModbusCollector::error(QAbstractSocket::SocketError)
{
    emit newLog(m_logFormat.arg(QString("ошибка соединения %1").arg(m_socket->errorString())));
}

QString CSModbusCollector::makeDumpLog(QByteArray &byteArray)
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

void CSModbusCollector::readyRead()
{
    QByteArray responce = m_socket->readAll();

    QString textDump= m_stationString + QString::number(m_stationId) + ":";
    textDump += makeDumpLog(responce);
    emit newDumpLog(textDump);

    QDataStream readStream(&responce, QIODevice::ReadOnly);
    QByteArray readData;

    quint8 stationId;
    readStream >> stationId;
    readData.append(stationId);
    if (stationId != m_stationId) {
        emit newLog(m_logFormat.arg("неверный номер станции"));
        //nextRequest();
        return;
    }

    quint8 command;
    readStream >> command;
    readData.append(command);

    if (stationId == m_stationId && command == 0x03) {
        quint8 bytesCount, registersCount;
        readStream >> bytesCount;
        readData.append(bytesCount);
        registersCount = bytesCount / 2;

        if (registersCount != m_registersCount) {
            emit newLog(m_logFormat.arg("неверная количество регистров"));
            //nextRequest();
            return;
        }

        CSRegistersArray registerArray;
        registerArray.startRegister = m_firstRegister;
        registerArray.stationAddress = m_stationId;
        for (int i=0; i < registersCount; ++i) {
            quint16 reg;
            readStream >> reg;
            readData.append(reg / 256);
            readData.append(reg % 256);
            registerArray.values.append(reg);
        }

        quint8 crc16_l, crc16_h;
        readStream >> crc16_l >> crc16_h;
        quint16 crc16 = crc16_l + ((quint16)crc16_h * 256);
        if (crc16 != calcCrc16(readData)) {
            emit newLog(m_logFormat.arg("неверная контрольная сумма"));
            //nextRequest();
            return;
        }

        emit newRegistersArray(registerArray);
    }
}

void CSModbusCollector::sendReadRequest()
{
    QByteArray request;

    request.append(m_stationId);

    request.append((quint8)0x03);

    //offset
    request.append((quint8)(m_firstRegister / 256));
    request.append((quint8)(m_firstRegister % 256));

    //registers count
    request.append((quint8)(m_registersCount / 256));
    request.append((quint8)(m_registersCount % 256));

    //crc
    quint16 crc = this->calcCrc16(request);
    request.append((quint8)(crc % 256));
    request.append((quint8)(crc / 256));

    m_socket->write(request);
    emit newDumpLog(makeDumpLog(request).prepend(m_stationString+QString::number(m_stationId)+">> "));
}

void CSModbusCollector::nextRequest()
{
    // Отправка значений регистров
    /*if (m_socket->state() != QTcpSocket::ConnectedState){
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
    }*/

    // Запрос на чтение регистров
    sendReadRequest();
}

/*quint32 CSFatekCollector::sendRegister(quint8 station, quint16 address, quint16 value)
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
}*/

quint16 CSModbusCollector::calcCrc16(const QByteArray &data)
{
    union {char b[2]; unsigned short w;} Sum;
    char shift_cnt;
    //QByteArray *ptrByte=data;
    int i=0;

    Sum.w=0xffffU;
    for(quint32 byte_cnt = data.size(); byte_cnt>0; byte_cnt--) {
        quint8 byte = data.at(i++);
        Sum.w = (unsigned short)((Sum.w/256U)*256U+((Sum.w%256U)^(byte)));

        for (shift_cnt=0; shift_cnt<8; shift_cnt++) {
            if((Sum.w&0x1)==1) {
                Sum.w=(unsigned short)((Sum.w>>1)^0xa001U);
            } else {
                Sum.w>>=1;
            }
        }
    }
    return Sum.w;
}

void CSModbusCollector::timerEvent(QTimerEvent *)
{
    if (m_socket->state() == QTcpSocket::UnconnectedState) {
        reconnect();
    } else {
        nextRequest();
    }
}

void CSModbusCollector::reconnect()
{
    if (m_socket->state() != QTcpSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
    }
    m_socket->connectToHost(m_ipAddress, m_port);
}

void CSModbusCollector::startRequesting()
{
    QString mes = QString("попытка соединения %1:%2").arg(m_ipAddress, QString::number(m_port));
    emit newLog(m_logFormat.arg(mes));
    m_socket->connectToHost(m_ipAddress, m_port);
    m_timerId = startTimer(m_requestPeriod);
}
