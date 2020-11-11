#include "cssettings.h"

CSSettings::CSSettings(const QString &path, QObject * parent) : QSettings(path, QSettings::IniFormat, parent)
{
    setIniCodec("UTF-8");
    QStringList list = allKeys();
    //Получить все имена пользователей
    for(QStringList::iterator i = list.begin(); i != list.end(); i++){
        if ((*i).right(9) == "/username")
            m_userNameNickHash.insert(value((*i)).toString(), (*i).section("/",0,0));
    }
}

CSSettings::~CSSettings()
{

}

void CSSettings::addUser(QString name, QString password)
{
    if (m_userNameNickHash.contains(name)){
        setValue(m_userNameNickHash[name]+"/password", password);
    } else {
        int i = 1;
        while(true){
            if (contains("user"+QString::number(i)+"/username")){
                i++;
                continue;
            }
            m_userNameNickHash.insert(name, "user"+QString::number(i));
            setValue(m_userNameNickHash[name]+"/username", name);
            setValue(m_userNameNickHash[name]+"/password", password);
        }
    }
}

void CSSettings::remUser(QString name)
{
    if (m_userNameNickHash.contains(name)){
        this->remove(m_userNameNickHash[name]);
        m_userNameNickHash.remove(name);
    }
}

QString CSSettings::getUserProp(const QString &name, const QString &prop)
{
    QString key = m_userNameNickHash[name]+"/"+prop;
    if (!contains(key))
        return "";
    return value(key).toString();
}

