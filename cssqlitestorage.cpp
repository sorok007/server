#include "cssqlitestorage.h"

CSSQLiteStorage::CSSQLiteStorage(QObject *parent) : QObject(parent)
{
    min1 = startTimer(30*1000);
    //min15 = startTimer(15*60*1000);
    //min30 = startTimer(30*60*1000);
    //min60 = startTimer(60*60*1000);
}

CSSQLiteStorage::~CSSQLiteStorage()
{

}

bool CSSQLiteStorage::getDataBase(QSqlDatabase& dbase, const QString &station)
{
    if (QSqlDatabase::contains(station))
        dbase = QSqlDatabase::database(station);
    else{
        dbase = QSqlDatabase::addDatabase("QSQLITE", station);
        dbase.setDatabaseName(m_path+station+".db");
        if (!dbase.open()){
            emit newLog("SQLite -> fault to open database");
            QSqlDatabase::removeDatabase(station);
            return false;
        }
    }
    return true;
}

QList<QHash<QString, QString> > CSSQLiteStorage::runQuery(const QString &station, const QString &sqlquery)
{
    QSqlDatabase dbase;
    QList<QHash<QString, QString> > ret;
    if (!getDataBase(dbase, station)){
        QHash<QString, QString> line;
        line.insert("sqlerror", "DB connect error");
        ret.append(line);
        return ret;
    }
    QSqlQuery query(dbase);
    bool r = query.exec(sqlquery);
    if (!r){
        QHash<QString, QString> line;
        line.insert("sqlerror", query.lastError().text());
        ret.append(line);
        dbase.close();
        return ret;
    }
    QSqlRecord rec = query.record();
    while (query.next()) {
        QHash<QString, QString> line;
        for(int i =0; i<rec.count(); i++){
            line.insert(rec.fieldName(i), query.value(i).toString());
        }
        ret.append(line);
    }
    return ret;
}

QList<CSRecord> CSSQLiteStorage::getRecords(const QString &station, const QString &name, const QString &from, const QString &to)
{
    QSqlDatabase dbase;
    if (!getDataBase(dbase, station))
        return QList<CSRecord>();
    QSqlQuery query(dbase);
    bool r;
    QString vto = to;
    QString vfrom = from;
    QString resp = "SELECT * FROM `" + name + "` WHERE `datetime` > '" + vfrom.replace("_"," ") +
            "' AND `datetime` < '" + vto.replace("_"," ") + "' ORDER BY `id` DESC";
    r = query.exec(resp);
    if (!r){
        dbase.close();
        return QList<CSRecord>();
    }
    QSqlRecord rec = query.record();
    QList<CSRecord> ret;
    while (query.next()) {
        QString datetime = query.value(rec.indexOf("datetime")).toString();
        QString value = query.value(rec.indexOf("value")).toString();
        CSRecord regv;
        regv.datetime = QDateTime::fromString(datetime, "yyyy-MM-dd HH:mm:ss");
        regv.value = value;
        ret.prepend(regv);
    }
    return ret;
}

QList<CSRecord> CSSQLiteStorage::getRecords(const QString &station, const QString &name, int deep)
{
    QString fullRegName = station+"."+name;
    if (m_nameDeepHash.contains(fullRegName) && m_nameDeepHash[fullRegName] >= deep)
        return m_nameValuesHash[fullRegName].mid(0, deep);
    QSqlDatabase dbase;
    if (!getDataBase(dbase, station))
        return m_nameValuesHash[fullRegName];
    QSqlQuery query(dbase);
    bool r;
    r = query.exec("SELECT * FROM `" + name + "` ORDER BY `id` DESC LIMIT " + QString::number(deep));
    if (!r){
        dbase.close();
        return m_nameValuesHash[fullRegName];
    }
    QSqlRecord rec = query.record();
    m_nameValuesHash[fullRegName].clear();
    while (query.next()) {
        QString datetime = query.value(rec.indexOf("datetime")).toString();
        QString value = query.value(rec.indexOf("value")).toString();
        CSRecord regv;
        regv.datetime = QDateTime::fromString(datetime, "yyyy-MM-dd HH:mm:ss");
        regv.value = value;
        m_nameValuesHash[fullRegName].append(regv);
    }
    m_nameDeepHash[fullRegName] = m_nameValuesHash[fullRegName].size();
    return m_nameValuesHash[fullRegName];
}

bool CSSQLiteStorage::isSame(const QString &station, const QString &name, const QString &value)
{
    QList<CSRecord> vals = getRecords(station, name, 1);
    return !vals.empty() && (vals.first().value == value);
}

bool CSSQLiteStorage::isEmpty(const QString &station, const QString &reg)
{
    QList<CSRecord> vals = getRecords(station, reg, 1);
    return vals.empty();
}

void CSSQLiteStorage::archiveTristate(const QString &station,
                                      const QString &name,
                                      bool on,
                                      const QString &state)
{
    if (isEmpty(station, name)){
        if (on){
            if (!state.isEmpty())
                archiveRecord(station, "mess", m_stationNames[station]+": "+state+";#FF6060", ToStorage);
            archiveRecord(station, name, "2", ToStorage); // авария
        } else {
            archiveRecord(station, name, "0", ToStorage);
        }
    } else {
        if (isSame(station, name, "0") && on){
            archiveRecord(station, name, "2", ToStorage); // авария
            if (!state.isEmpty())
                archiveRecord(station, "mess", m_stationNames[station]+": "+state+";#FF6060", ToStorage);
        } else if (!isSame(station, name, "0") && !on){
            archiveRecord(station, name, "0", ToStorage); // в норму
            if (!state.isEmpty())
                archiveRecord(station, "mess", m_stationNames[station]+": Состояние \""+state+"\" вернулось в норму;#60FF60", ToStorage);
        }
    }
}

void CSSQLiteStorage::roundIntervals()
{
    foreach(QString fullName, namesWithIntervals){
        QString station = fullName.section(".", 0, 0);
        QString name = fullName.section(".", 1);
        //1 minute
        if (valuesWithIntervals.contains(fullName+"1min") && countsWithIntervals.contains(fullName+"1min")){
            if (countsWithIntervals[fullName+"1min"] > 0){
                QDateTime datetime = QDateTime::currentDateTime();
                int s = datetime.toString("ss").toInt();
                datetime = datetime.addSecs(-s);
                QString sdatetime = datetime.toString("yyyy-MM-dd HH:mm:ss");
                QList<CSRecord> vals = getRecords(station, name+"1min", 1);
                QString csdatetime;
                if (!vals.empty())
                    csdatetime = vals.first().datetime.toString("yyyy-MM-dd HH:mm:ss");
                if (vals.empty() || csdatetime != sdatetime){
                    double v = valuesWithIntervals[fullName+"1min"] / countsWithIntervals[fullName+"1min"];
                    archiveRecord(station, name+"1min", QString::number(v, 'f', 2)+datetime.toString("|yyyy-MM-dd HH:mm:ss"), ToStorage);
                    countsWithIntervals[fullName+"1min"] = 0;
                    valuesWithIntervals[fullName+"1min"] = 0;
                }
            }
        }
        //15 minutes
        if (valuesWithIntervals.contains(fullName+"15min") && countsWithIntervals.contains(fullName+"15min")){
            if (countsWithIntervals[fullName+"15min"] > 0){
                QDateTime datetime = QDateTime::currentDateTime();
                int s = datetime.toString("ss").toInt();
                int m = datetime.toString("mm").toInt();
                datetime = datetime.addSecs(-(s+60*(m%15)));
                QString sdatetime = datetime.toString("yyyy-MM-dd HH:mm:ss");
                QList<CSRecord> vals = getRecords(station, name+"15min", 1);
                QString csdatetime;
                if (!vals.empty())
                    csdatetime = vals.first().datetime.toString("yyyy-MM-dd HH:mm:ss");
                if (vals.empty() || csdatetime != sdatetime){
                    double v = valuesWithIntervals[fullName+"15min"] / countsWithIntervals[fullName+"15min"];
                    archiveRecord(station, name+"15min", QString::number(v, 'f', 2)+datetime.toString("|yyyy-MM-dd HH:mm:ss"), ToStorage);
                    countsWithIntervals[fullName+"15min"] = 0;
                    valuesWithIntervals[fullName+"15min"] = 0;
                }
            }
        }
        //30 minutes
        if (valuesWithIntervals.contains(fullName+"30min") && countsWithIntervals.contains(fullName+"30min")){
            if (countsWithIntervals[fullName+"30min"] > 0){
                QDateTime datetime = QDateTime::currentDateTime();
                int s = datetime.toString("ss").toInt();
                int m = datetime.toString("mm").toInt();
                datetime = datetime.addSecs(-(s+60*(m%30)));
                QString sdatetime = datetime.toString("yyyy-MM-dd HH:mm:ss");
                QList<CSRecord> vals = getRecords(station, name+"30min", 1);
                QString csdatetime;
                if (!vals.empty())
                    csdatetime = vals.first().datetime.toString("yyyy-MM-dd HH:mm:ss");
                if (vals.empty() || csdatetime != sdatetime){
                    double v = valuesWithIntervals[fullName+"30min"] / countsWithIntervals[fullName+"30min"];
                    archiveRecord(station, name+"30min", QString::number(v, 'f', 2)+datetime.toString("|yyyy-MM-dd HH:mm:ss"), ToStorage);
                    countsWithIntervals[fullName+"30min"] = 0;
                    valuesWithIntervals[fullName+"30min"] = 0;
                }
            }
        }
        //60 minutes
        if (valuesWithIntervals.contains(fullName+"60min") && countsWithIntervals.contains(fullName+"60min")){
            if (countsWithIntervals[fullName+"60min"] > 0){
                QDateTime datetime = QDateTime::currentDateTime();
                int s = datetime.toString("ss").toInt();
                int m = datetime.toString("mm").toInt();
                datetime = datetime.addSecs(-(s+60*m));
                QString sdatetime = datetime.toString("yyyy-MM-dd HH:mm:ss");
                QList<CSRecord> vals = getRecords(station, name+"60min", 1);
                QString csdatetime;
                if (!vals.empty())
                    csdatetime = vals.first().datetime.toString("yyyy-MM-dd HH:mm:ss");
                if (vals.empty() || csdatetime != sdatetime){
                    double v = valuesWithIntervals[fullName+"60min"] / countsWithIntervals[fullName+"60min"];
                    archiveRecord(station, name+"60min", QString::number(v, 'f', 2)+datetime.toString("|yyyy-MM-dd HH:mm:ss"), ToStorage);
                    countsWithIntervals[fullName+"60min"] = 0;
                    valuesWithIntervals[fullName+"60min"] = 0;
                }
            }
        }
    }
}

void CSSQLiteStorage::roundIntervalsTechnomer(QString fullName, QDateTime _datetime, double value)
{
    QString station = fullName.section(".", 0, 0);
    QString name = fullName.section(".", 1);

    QStringList suff;
    suff.append("1min");
    suff.append("15min");
    suff.append("30min");
    suff.append("60min");
    foreach(QString suf, suff){
        if (!countsWithIntervals.contains(station+"."+name+suf))
            countsWithIntervals[station+"."+name+suf] = 0;
        if (!valuesWithIntervals.contains(station+"."+name+suf))
            valuesWithIntervals[station+"."+name+suf] = 0;
        countsWithIntervals[station+"."+name+suf]++;
        valuesWithIntervals[station+"."+name+suf] += value;
    }

    //1 minute
    if (valuesWithIntervals.contains(fullName+"1min") && countsWithIntervals.contains(fullName+"1min")){
        if (countsWithIntervals[fullName+"1min"] > 0){
            QDateTime datetime = _datetime;//QDateTime::currentDateTime();
            int s = datetime.toString("ss").toInt();
            datetime = datetime.addSecs(-s);
            QString sdatetime = datetime.toString("yyyy-MM-dd HH:mm:ss");
            QList<CSRecord> vals = getRecords(station, name+"1min", 1);
            QString csdatetime;
            if (!vals.empty())
                csdatetime = vals.first().datetime.toString("yyyy-MM-dd HH:mm:ss");
            if (vals.empty() || csdatetime != sdatetime){
                double v = valuesWithIntervals[fullName+"1min"] / countsWithIntervals[fullName+"1min"];
                archiveRecord(station, name+"1min", QString::number(v, 'f', 2)+datetime.toString("|yyyy-MM-dd HH:mm:ss"), ToStorage);
                countsWithIntervals[fullName+"1min"] = 0;
                valuesWithIntervals[fullName+"1min"] = 0;
            }
        }
    }
    //15 minutes
    if (valuesWithIntervals.contains(fullName+"15min") && countsWithIntervals.contains(fullName+"15min")){
        if (countsWithIntervals[fullName+"15min"] > 0){
            QDateTime datetime = _datetime;//QDateTime::currentDateTime();
            int s = datetime.toString("ss").toInt();
            int m = datetime.toString("mm").toInt();
            datetime = datetime.addSecs(-(s+60*(m%15)));
            QString sdatetime = datetime.toString("yyyy-MM-dd HH:mm:ss");
            QList<CSRecord> vals = getRecords(station, name+"15min", 1);
            QString csdatetime;
            if (!vals.empty())
                csdatetime = vals.first().datetime.toString("yyyy-MM-dd HH:mm:ss");
            if (vals.empty() || csdatetime != sdatetime){
                double v = valuesWithIntervals[fullName+"15min"] / countsWithIntervals[fullName+"15min"];
                archiveRecord(station, name+"15min", QString::number(v, 'f', 2)+datetime.toString("|yyyy-MM-dd HH:mm:ss"), ToStorage);
                countsWithIntervals[fullName+"15min"] = 0;
                valuesWithIntervals[fullName+"15min"] = 0;
            }
        }
    }
    //30 minutes
    if (valuesWithIntervals.contains(fullName+"30min") && countsWithIntervals.contains(fullName+"30min")){
        if (countsWithIntervals[fullName+"30min"] > 0){
            QDateTime datetime = _datetime;//QDateTime::currentDateTime();
            int s = datetime.toString("ss").toInt();
            int m = datetime.toString("mm").toInt();
            datetime = datetime.addSecs(-(s+60*(m%30)));
            QString sdatetime = datetime.toString("yyyy-MM-dd HH:mm:ss");
            QList<CSRecord> vals = getRecords(station, name+"30min", 1);
            QString csdatetime;
            if (!vals.empty())
                csdatetime = vals.first().datetime.toString("yyyy-MM-dd HH:mm:ss");
            if (vals.empty() || csdatetime != sdatetime){
                double v = valuesWithIntervals[fullName+"30min"] / countsWithIntervals[fullName+"30min"];
                archiveRecord(station, name+"30min", QString::number(v, 'f', 2)+datetime.toString("|yyyy-MM-dd HH:mm:ss"), ToStorage);
                countsWithIntervals[fullName+"30min"] = 0;
                valuesWithIntervals[fullName+"30min"] = 0;
            }
        }
    }
    //60 minutes
    if (valuesWithIntervals.contains(fullName+"60min") && countsWithIntervals.contains(fullName+"60min")){
        if (countsWithIntervals[fullName+"60min"] > 0){
            QDateTime datetime = _datetime;//QDateTime::currentDateTime();
            int s = datetime.toString("ss").toInt();
            int m = datetime.toString("mm").toInt();
            datetime = datetime.addSecs(-(s+60*m));
            QString sdatetime = datetime.toString("yyyy-MM-dd HH:mm:ss");
            QList<CSRecord> vals = getRecords(station, name+"60min", 1);
            QString csdatetime;
            if (!vals.empty())
                csdatetime = vals.first().datetime.toString("yyyy-MM-dd HH:mm:ss");
            if (vals.empty() || csdatetime != sdatetime){
                double v = valuesWithIntervals[fullName+"60min"] / countsWithIntervals[fullName+"60min"];
                archiveRecord(station, name+"60min", QString::number(v, 'f', 2)+datetime.toString("|yyyy-MM-dd HH:mm:ss"), ToStorage);
                countsWithIntervals[fullName+"60min"] = 0;
                valuesWithIntervals[fullName+"60min"] = 0;
            }
        }
    }
}

void CSSQLiteStorage::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == min1)
        roundIntervals();
    return;
    /*foreach(QString fullName, namesWithIntervals){
        QString station = fullName.section(".", 0, 0);
        QString name = fullName.section(".", 1);
        if (event->timerId() == min1){
            if (!valuesWithIntervals.contains(fullName+"1min") || !countsWithIntervals.contains(fullName+"1min"))
                continue; // тогда другие интервалы не запишутся!!!!
            if (countsWithIntervals[fullName+"1min"] > 0){
                double v = valuesWithIntervals[fullName+"1min"] / countsWithIntervals[fullName+"1min"];
                archiveRecord(station, name+"1min", v, ToStorage);
            }
            countsWithIntervals[fullName+"1min"] = 0;
            valuesWithIntervals[fullName+"1min"] = 0;
        }
        if (event->timerId() == min15){
            if (!valuesWithIntervals.contains(fullName+"15min") || !countsWithIntervals.contains(fullName+"15min"))
                continue;
            if (countsWithIntervals[fullName+"15min"] > 0){
                double v = valuesWithIntervals[fullName+"15min"] / countsWithIntervals[fullName+"15min"];
                archiveRecord(station, name+"15min", v, ToStorage);
            }
            countsWithIntervals[fullName+"15min"] = 0;
            valuesWithIntervals[fullName+"15min"] = 0;
        }
        if (event->timerId() == min30){
            if (!valuesWithIntervals.contains(fullName+"30min") || !countsWithIntervals.contains(fullName+"30min"))
                continue;
            if (countsWithIntervals[fullName+"30min"] > 0){
                double v = valuesWithIntervals[fullName+"30min"] / countsWithIntervals[fullName+"30min"];
                archiveRecord(station, name+"30min", v, ToStorage);
            }
            countsWithIntervals[fullName+"30min"] = 0;
            valuesWithIntervals[fullName+"30min"] = 0;
        }
        if (event->timerId() == min60){
            if (!valuesWithIntervals.contains(fullName+"60min") || !countsWithIntervals.contains(fullName+"60min"))
                continue;
            if (countsWithIntervals[fullName+"60min"] > 0){
                double v = valuesWithIntervals[fullName+"60min"] / countsWithIntervals[fullName+"60min"];
                archiveRecord(station, name+"60min", v, ToStorage);
            }
            countsWithIntervals[fullName+"60min"] = 0;
            valuesWithIntervals[fullName+"60min"] = 0;
        }
    }*/
}

void CSSQLiteStorage::archiveRecord(const QString &station,
                                 const QString &name,
                                 const QString &_value,
                                 int mode,
                                 int periodSec)
{
    QString value = _value;
    if (namesWithIntervals.contains(station+"."+name)){
        QStringList suff;
        suff.append("1min");
        suff.append("15min");
        suff.append("30min");
        suff.append("60min");
        foreach(QString suf, suff){
            if (!countsWithIntervals.contains(station+"."+name+suf))
                countsWithIntervals[station+"."+name+suf] = 0;
            if (!valuesWithIntervals.contains(station+"."+name+suf))
                valuesWithIntervals[station+"."+name+suf] = 0;
            countsWithIntervals[station+"."+name+suf]++;
            valuesWithIntervals[station+"."+name+suf] += value.toDouble();
        }
    }
    if (((mode & OnEvent) == OnChange) && isSame(station, name, value))
            return;

    QDateTime currents = QDateTime::currentDateTime();
    QString datetime = currents.toString("yyyy-MM-dd HH:mm:ss");
    if (!value.section("|", 1).isEmpty()){
        datetime = value.section("|",1);
        value = value.section("|",0,0);
    }
    QString fullRegName = station+"."+name;
    emit newValue(fullRegName, QDateTime::fromString(datetime, "yyyy-MM-dd HH:mm:ss").toString("dd.MM.yyyy HH:mm:ss")+"|"+value);

    if ((mode & OnEvent) == OnPeriod){
        QList<CSRecord> vals = getRecords(station, name, 1);
        if (!vals.empty() && (vals.first().datetime.addSecs(periodSec) > QDateTime::currentDateTime()))
            return;
    }
    CSRecord regv;
    //regv.datetime = currents;
    regv.datetime = QDateTime::fromString(datetime, "yyyy-MM-dd HH:mm:ss");
    regv.value = value;
    m_nameValuesHash[fullRegName].prepend(regv);
    if (m_nameDeepHash.contains(fullRegName)){
        while(m_nameValuesHash[fullRegName].size() > m_nameDeepHash[fullRegName])
            m_nameValuesHash[fullRegName].removeLast();
    } else {
        m_nameDeepHash[fullRegName] = 1;
    }
    if (!(mode & ToStorage))
        return;
    QSqlDatabase dbase;
    if (!getDataBase(dbase, station))
        return;
    QSqlQuery query(dbase);
    bool r;
    r = query.exec("CREATE TABLE IF NOT EXISTS `" + name +
               "` (`id` INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, `datetime` DATETIME, `value` TEXT)");
    if (!r){
        emit newLog("SQLite -> Query error on create table");
        return;
    }
    r = query.exec("CREATE INDEX IF NOT EXISTS `timeind_" + name + "` on `" + name + "` (datetime)");
    if (!r){
        emit newLog("SQLite -> Query error on create index");
        return;
    }
    r = query.exec("INSERT INTO " + name + "(datetime, value) values('" + datetime + "','" + value + "')");
    if (!r){
        emit newLog("SQLite -> Query error on insert into");
        return;
    }
}

void CSSQLiteStorage::archiveRecord(const QString &station,
                                 const QString &name,
                                 int value,
                                 int mode,
                                 int periodSec)
{
    archiveRecord(station, name, QString::number(value), mode, periodSec);
}

void CSSQLiteStorage::archiveRecord(const QString &station,
                                 const QString &name,
                                 double value,
                                 int mode,
                                 int periodSec)
{
    archiveRecord(station, name, QString::number(value), mode, periodSec);
}
