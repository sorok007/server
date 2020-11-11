#ifndef CSSETTINGS_H
#define CSSETTINGS_H

#include <QSettings>
#include <QHash>

class CSSettings : public QSettings
{
    Q_OBJECT
public:
    explicit CSSettings(const QString &path, QObject * parent);
    ~CSSettings();
    void addUser(QString name, QString password);
    void remUser(QString name);
    QHash<QString, QString> m_userNameNickHash;
    QString getUserProp(const QString &name, const QString& prop);
};

#endif // CSSETTINGS_H
