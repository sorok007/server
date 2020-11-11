#ifndef CSTYPES_H
#define CSTYPES_H

#include <QString>
#include <QVector>
#include <QDateTime>

//Вспомогательное объединение для чтения пакета modbus
union Word16
{
    quint16 w;
    struct{
        quint8 wl, wh;
    };
};

//Массив нумерованных регистров
struct CSRegistersArray{
    quint8 stationAddress;
    quint16 startRegister;
    QVector<quint16> values;
    bool isWithRegister(int reg) const {
        return startRegister <= reg && reg < (startRegister + values.size());
    }
    quint16 getRegister(int reg) const {
        return values.value(reg - startRegister);
    }
    quint8 getAsciiRegister(int reg) const {
        Word16 wd;
        wd.w = getRegister(reg);
        QString str;
        str.append(QChar(wd.wh)).append(QChar(wd.wl));
        return str.toInt();
    }
    QString stationString(const QString &station = "station"){
        return station + QString::number(stationAddress);
    }
};


//Датированная запись
struct CSRecord{
    QString datetimeToString(){
        return datetime.toString("dd.MM.yyyy HH:mm:ss");
    }
    QDateTime datetime;
    QString value;
};

//Параметры записи в БД
#define     None        0x0
#define     ToStorage   0x1
#define     OnChange    0x2
#define     OnPeriod    0x4
#define     OnEvent     0x6

#endif // CSTYPES_H

