#ifndef CSSQLITESTORAGE_H
#define CSSQLITESTORAGE_H

#include <QObject>
#include <QtSql>
#include <QList>
#include <QHash>
#include "cstypes.h"

class CSSQLiteStorage : public QObject
{
    Q_OBJECT
public:
    explicit CSSQLiteStorage(QObject *parent = 0);
    virtual ~CSSQLiteStorage();
    void archiveRecord(const QString &station,
                         const QString &name,
                         const QString &value,
                         int mode = None,
                         int periodSec = 0);
    void archiveRecord(const QString &station,
                         const QString &name,
                         int value,
                         int mode = None,
                         int periodSec = 0);
    void archiveRecord(const QString &station,
                         const QString &name,
                         double value,
                         int mode = None,
                         int periodSec = 0);
    void archiveTristate(const QString &station,
                         const QString &name,
                         bool on,
                         const QString &state = "");
    // Совпадает ли с последним записанным значением
    bool isSame(const QString &station, const QString &name, const QString &value);
    // Существует ли запись с таким именем
    bool isEmpty(const QString &station, const QString &name);
    // Получить список последних записей в количестве deep
    QList<CSRecord> getRecords(const QString &station, const QString &name, int deep=1);
    // Получить список записей между интервалами времени
    QList<CSRecord> getRecords(const QString &station, const QString &name, const QString &from, const QString &to);
    // Установить директорию хранения
    void setPath(const QString &path){ m_path = path; }
    // Выполнить произвольную SQL инструкцию
    QList<QHash<QString, QString> > runQuery(const QString &station, const QString &sqlquery);
    void setStationNames(const QHash<QString, QString> &stationNames) { m_stationNames = stationNames; }
    void addNameWithIntervals(const QString& fullName) { namesWithIntervals.insert(fullName); }
    void roundIntervals();
    void roundIntervalsTechnomer(QString fullName, QDateTime _datetime, double value);

public slots:

protected:
    void timerEvent(QTimerEvent*);

private:
    int min1, min15, min30, min60;
    QHash<QString, double> valuesWithIntervals;
    QHash<QString, int> countsWithIntervals;
    QSet<QString> namesWithIntervals;
    bool getDataBase(QSqlDatabase& dbase, const QString &station);
    QHash<QString, QList<CSRecord> > m_nameValuesHash;
    QHash<QString, int> m_nameDeepHash;
    QString m_path;
    QHash<QString, QString> m_stationNames;

signals:
    void newLog(const QString &log);
    void newValue(const QString &fullName, const QString &fullValue);
};

#endif // CSSQLITESTORAGE_H
