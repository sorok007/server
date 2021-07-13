#include <QStatusBar>
#include <QDataStream>
#include <QSerialPort>
#include <QSerialPortInfo>
#include "server.h"
#include "cssupervisorserver.h"
#include "ui_bore.h"
#include "ui_smsform.h"
#include "smsnotifications.h"

enum X16_BIT_MASKS {
    MASK_0 = 0x01,
    MASK_1 = 0x02,
    MASK_2 = 0x04,
    MASK_3 = 0x08,
    MASK_4 = 0x10,
    MASK_5 = 0x20,
    MASK_6 = 0x40,
    MASK_7 = 0x80,
    MASK_8 = 0x100,
    MASK_9 = 0x200,
    MASK_10 = 0x400,
    MASK_11 = 0x800,
    MASK_12 = 0x1000,
    MASK_13 = 0x2000,
    MASK_14 = 0x4000,
    MASK_15 = 0x8000
};

struct PumpStationDefinition
{
    QString name;
    int stationId;
    int firstRegister;
    int registerCount;
};

float getFloatValueFrom2Regs(const CSRegistersArray &registerArray, int reg1, int reg2) {
    Word16 w1, w2;
    w1.w= registerArray.getRegister(reg1);
    w2.w = registerArray.getRegister(reg2);

    int val0 = w1.wl;
    int val1 = w1.wh;
    int val2 = w2.wl;
    int val3 = w2.wh;

    int value = val0 | (val1 << 8) | (val2 << 16) | (val3 << 24);
    return  *((float*) &value);
}

Server::Server(QObject *parent) : QObject(parent)
{
    m_stationNames.insert("vns1", "ВНС1");
    m_stationNames.insert("vns2", "ВНС2");
    m_stationNames.insert("vns3", "ВНС3");
    m_stationNames.insert("vns4", "ВНС4");
    m_stationNames.insert("vns5", "ВНС5");
    m_stationNames.insert("vns6", "ВНС6");
    m_stationNames.insert("vns7", "ВНС7");
    m_stationNames.insert("vns8", "ВНС8");
    m_stationNames.insert("_bs", "Лодочная станция");
    m_stationNames.insert("kns1", "КНС1");
    m_stationNames.insert("kns3", "КНС3");
    m_stationNames.insert("kns4", "КНС4");
    m_stationNames.insert("kns5", "КНС5");
    m_stationNames.insert("kns6", "КНС6");
    m_stationNames.insert("kns7", "КНС7");
    m_stationNames.insert("kns8", "КНС8");
    m_stationNames.insert("kns9", "КНС9");
    m_stationNames.insert("kns10", "КНС10");
    m_stationNames.insert("kns11", "КНС11");
    m_stationNames.insert("kns12", "КНС12");
    m_stationNames.insert("kns13", "КНС13");
    m_stationNames.insert("kns14", "КНС14");
    m_stationNames.insert("kns15", "КНС15");
    m_stationNames.insert("kns16", "КНС16");
    m_stationNames.insert("kns17", "КНС17");
    m_stationNames.insert("kns18", "КНС18");

    m_stationNames.insert("bore1", "Скважина 1");
    m_stationNames.insert("bore2", "Скважина 2");
    m_stationNames.insert("bore3", "Скважина 3");
    m_stationNames.insert("bore4", "Скважина 4");
    m_stationNames.insert("bore5", "Скважина 5");
    m_stationNames.insert("bore6", "Скважина 6");
    m_stationNames.insert("bore7", "Скважина 7");
    m_stationNames.insert("bore8", "Скважина 8");
    m_stationNames.insert("bore9", "Скважина 9");
    m_stationNames.insert("bore10", "Скважина 10");
    m_stationNames.insert("bore11", "Скважина 11");
    m_stationNames.insert("bore12", "Скважина 12");
    m_stationNames.insert("bore13", "Скважина 13");
    m_stationNames.insert("bore14", "Скважина 14");
    m_stationNames.insert("bore16", "Скважина 16");
    m_stationNames.insert("bore28", "Скважина 28");

    m_stationNames.insert("kosk", "КОСК");

    m_settings = new CSSettings("settings.ini", this);
    m_mainWindow.show();

    connect(m_mainWindow.smsForm->ui->sendSMSButton, SIGNAL(clicked(bool)), SLOT(sendTestSMS()));

    setStorage();
    setSupervisorServer();
    setModbusServer();
    setBores();
    setKosk();

    m_vnsFatekDefinitions = new QVector<PumpStationDefinition>();
    m_vnsFatekDefinitions->push_back({QString("vns1"), 21, 0, 28});
    m_vnsFatekDefinitions->push_back({QString("vns2"), 2, 0, 31});
    m_vnsFatekDefinitions->push_back({QString("vns3"), 20, 0, 30});
    m_vnsFatekDefinitions->push_back({QString("vns4"), 4, 0, 28});
    m_vnsFatekDefinitions->push_back({QString("vns5"), 1, 0, 29});
    m_vnsFatekDefinitions->push_back({QString("vns6"), 3, 0, 40});
    m_vnsFatekDefinitions->push_back({QString("vns7"), 7, 0, 31});
    m_vnsFatekDefinitions->push_back({QString("vns8"), -1, 0, 12});
    m_vnsFatekDefinitions->push_back({QString("_bs"), -1, 0, 10});

    /*foreach (PumpStationDefinition def, *m_vnsFatekDefinitions) {
        this->setVNS(def);
    }*/
    for(int i = 0; i < m_vnsFatekDefinitions->size(); ++i) {
        this->setVNS(m_vnsFatekDefinitions->operator [](i));
    }

    m_knsFatekDefinitions = new QVector<PumpStationDefinition>();
    for (int i=1;i<=16;++i) {
        if (i == 2) continue; //kns2

        this->setFatekKNS(QString("kns%1").arg(i));
    }

    m_knsModbusDefinitions = new QVector<PumpStationDefinition>();
    for (int i=17;i<=18;++i) {
        this->setModbusKNS(QString("kns%1").arg(i));
    }

    //Интервальные техномера
    //d8, d9, d10, qsliz, r4, qverig, r5, qkrasnoe, r6, qviezd1, r7, qviezd2, r8, qkirill, r10, qgorod, d7, r9
    namesWithIntervalsTechnomer.insert("technomer.d8");
    namesWithIntervalsTechnomer.insert("technomer.d9");
    namesWithIntervalsTechnomer.insert("technomer.d10");
    namesWithIntervalsTechnomer.insert("technomer.qsliz");
    namesWithIntervalsTechnomer.insert("technomer.r4");
    namesWithIntervalsTechnomer.insert("technomer.qverig");
    namesWithIntervalsTechnomer.insert("technomer.r5");
    namesWithIntervalsTechnomer.insert("technomer.qkrasnoe");
    namesWithIntervalsTechnomer.insert("technomer.r6");
    namesWithIntervalsTechnomer.insert("technomer.qviezd1");
    namesWithIntervalsTechnomer.insert("technomer.r7");
    namesWithIntervalsTechnomer.insert("technomer.qviezd2");
    namesWithIntervalsTechnomer.insert("technomer.r8");
    namesWithIntervalsTechnomer.insert("technomer.qkirill");
    namesWithIntervalsTechnomer.insert("technomer.r10");
    namesWithIntervalsTechnomer.insert("technomer.qgorod");
    namesWithIntervalsTechnomer.insert("technomer.d7");
    namesWithIntervalsTechnomer.insert("technomer.r9");
    namesWithIntervalsTechnomer.insert("technomer.r9198_flow");
    namesWithIntervalsTechnomer.insert("technomer.r9199_flow");
    namesWithIntervalsTechnomer.insert("technomer.r9191_flow");
    namesWithIntervalsTechnomer.insert("technomer.r9187_flow");
    namesWithIntervalsTechnomer.insert("technomer.r9193_flow");
    namesWithIntervalsTechnomer.insert("technomer.r9194_flow");
    namesWithIntervalsTechnomer.insert("technomer.r9188_flow");
    namesWithIntervalsTechnomer.insert("technomer.r9195_flow");
    namesWithIntervalsTechnomer.insert("technomer.r9186_flow");
    namesWithIntervalsTechnomer.insert("technomer.r9190_flow");
    namesWithIntervalsTechnomer.insert("technomer.r9189_flow");
    namesWithIntervalsTechnomer.insert("technomer.r9197_flow");
    namesWithIntervalsTechnomer.insert("technomer.r9103_flow");
    namesWithIntervalsTechnomer.insert("technomer.r9201_flow");
    namesWithIntervalsTechnomer.insert("technomer.r9200_flow");

    startTimer(10000); //Проверка потери связи и бездействия насосов
    m_startingTime = QDateTime::currentDateTime();
    m_sqlstorage->archiveRecord("vns1", "linkState", "0");
    m_sqlstorage->archiveRecord("vns2", "linkState", "0");
    m_sqlstorage->archiveRecord("vns3", "linkState", "0");
    m_sqlstorage->archiveRecord("vns4", "linkState", "0");
    m_sqlstorage->archiveRecord("vns5", "linkState", "0");
    m_sqlstorage->archiveRecord("vns6", "linkState", "0");
    m_sqlstorage->archiveRecord("vns7", "linkState", "0");
    m_sqlstorage->archiveRecord("vns8", "linkState", "0");
    m_sqlstorage->archiveRecord("kns1", "linkState", "0");
    m_sqlstorage->archiveRecord("kns3", "linkState", "0");
    m_sqlstorage->archiveRecord("kns4", "linkState", "0");
    m_sqlstorage->archiveRecord("kns5", "linkState", "0");
    m_sqlstorage->archiveRecord("kns6", "linkState", "0");
    m_sqlstorage->archiveRecord("kns7", "linkState", "0");
    m_sqlstorage->archiveRecord("kns8", "linkState", "0");
    m_sqlstorage->archiveRecord("kns9", "linkState", "0");
    m_sqlstorage->archiveRecord("kns10", "linkState", "0");
    m_sqlstorage->archiveRecord("kns11", "linkState", "0");
    m_sqlstorage->archiveRecord("kns12", "linkState", "0");
    m_sqlstorage->archiveRecord("kns13", "linkState", "0");
    m_sqlstorage->archiveRecord("kns14", "linkState", "0");
    m_sqlstorage->archiveRecord("kns15", "linkState", "0");
    m_sqlstorage->archiveRecord("kns16", "linkState", "0");
    m_sqlstorage->archiveRecord("kns17", "linkState", "0");
    m_sqlstorage->archiveRecord("kns18", "linkState", "0");

    m_sqlstorage->archiveRecord("bore1", "linkState", "0");
    m_sqlstorage->archiveRecord("bore2", "linkState", "0");
    m_sqlstorage->archiveRecord("bore3", "linkState", "0");
    m_sqlstorage->archiveRecord("bore4", "linkState", "0");
    m_sqlstorage->archiveRecord("bore5", "linkState", "0");
    m_sqlstorage->archiveRecord("bore6", "linkState", "0");
    m_sqlstorage->archiveRecord("bore7", "linkState", "0");
    m_sqlstorage->archiveRecord("bore8", "linkState", "0");
    m_sqlstorage->archiveRecord("bore9", "linkState", "0");
    m_sqlstorage->archiveRecord("bore10", "linkState", "0");
    m_sqlstorage->archiveRecord("bore11", "linkState", "0");
    m_sqlstorage->archiveRecord("bore12", "linkState", "0");
    m_sqlstorage->archiveRecord("bore13", "linkState", "0");
    m_sqlstorage->archiveRecord("bore14", "linkState", "0");
    m_sqlstorage->archiveRecord("bore16", "linkState", "0");
    m_sqlstorage->archiveRecord("bore28", "linkState", "0");

    m_sqlstorage->archiveRecord("vns2", "noneState", "0");
    m_sqlstorage->archiveRecord("vns6", "noneState", "0");
    m_sqlstorage->archiveRecord("kns1", "noneState", "0");
    m_sqlstorage->archiveRecord("kns3", "noneState", "0");
    m_sqlstorage->archiveRecord("kns4", "noneState", "0");
    m_sqlstorage->archiveRecord("kns5", "noneState", "0");
    m_sqlstorage->archiveRecord("kns6", "noneState", "0");
    m_sqlstorage->archiveRecord("kns7", "noneState", "0");
    m_sqlstorage->archiveRecord("kns8", "noneState", "0");
    m_sqlstorage->archiveRecord("kns9", "noneState", "0");
    m_sqlstorage->archiveRecord("kns10", "noneState", "0");
    m_sqlstorage->archiveRecord("kns11", "noneState", "0");
    m_sqlstorage->archiveRecord("kns12", "noneState", "0");
    m_sqlstorage->archiveRecord("kns13", "noneState", "0");
    m_sqlstorage->archiveRecord("kns14", "noneState", "0");
    m_sqlstorage->archiveRecord("kns15", "noneState", "0");
    m_sqlstorage->archiveRecord("kns16", "noneState", "0");
    m_sqlstorage->archiveRecord("kns17", "noneState", "0");
    m_sqlstorage->archiveRecord("kns18", "noneState", "0");

    if (m_sqlstorage->isEmpty("vns1", "pressureMap")) {
        m_sqlstorage->archiveRecord("vns1", "pressureMap", "");
    }
    if (m_sqlstorage->isEmpty("vns2", "pressureMap")) {
        m_sqlstorage->archiveRecord("vns2", "pressureMap", "");
    }
    if (m_sqlstorage->isEmpty("vns4", "pressureMap")) {
        m_sqlstorage->archiveRecord("vns4", "pressureMap", "");
    }
    if (m_sqlstorage->isEmpty("vns5", "pressureMap")) {
        m_sqlstorage->archiveRecord("vns5", "pressureMap", "");
    }
    if (m_sqlstorage->isEmpty("vns6", "pressureMap")) {
        m_sqlstorage->archiveRecord("vns6", "pressureMap", "");
    }
    if (m_sqlstorage->isEmpty("vns7", "pressureMap")) {
        m_sqlstorage->archiveRecord("vns7", "pressureMap", "");
    }

    foreach(QString station, m_stationNames.keys()){
        m_sqlstorage->addNameWithIntervals(station+".curValue");
        m_sqlstorage->addNameWithIntervals(station+".curPerValue");
        m_sqlstorage->addNameWithIntervals(station+".presInValue");
        m_sqlstorage->addNameWithIntervals(station+".presOutValue");
        m_sqlstorage->addNameWithIntervals(station+".m1Value");
        m_sqlstorage->addNameWithIntervals(station+".m2Value");
        m_sqlstorage->addNameWithIntervals(station+".msValue");
        m_sqlstorage->addNameWithIntervals(station+".mh1Value");
        m_sqlstorage->addNameWithIntervals(station+".mh2Value");
        m_sqlstorage->addNameWithIntervals(station+".mhsValue");
        m_sqlstorage->addNameWithIntervals(station+".freqValue");
        m_sqlstorage->addNameWithIntervals(station+".waterLevelValue");
    }

    m_sqlstorage->addNameWithIntervals("vns6.mInValue");
    m_sqlstorage->addNameWithIntervals("vns6.mhInValue");
    m_sqlstorage->addNameWithIntervals("vns6.waterLevel1Value");
    m_sqlstorage->addNameWithIntervals("vns6.waterLevel2Value");

    m_sqlstorage->addNameWithIntervals("vns2.waterLevel1Value");
    m_sqlstorage->addNameWithIntervals("vns2.waterLevel2Value");

    m_sqlstorage->addNameWithIntervals("vns7.cur1Value");
    m_sqlstorage->addNameWithIntervals("vns7.freq1Value");
    m_sqlstorage->addNameWithIntervals("vns7.curPer1Value");
    m_sqlstorage->addNameWithIntervals("vns7.cur2Value");
    m_sqlstorage->addNameWithIntervals("vns7.freq2Value");
    m_sqlstorage->addNameWithIntervals("vns7.curPer2Value");
    m_sqlstorage->addNameWithIntervals("vns7.cur3Value");
    m_sqlstorage->addNameWithIntervals("vns7.freq3Value");
    m_sqlstorage->addNameWithIntervals("vns7.curPer3Value");
    m_sqlstorage->addNameWithIntervals("vns7.mValue");
    m_sqlstorage->addNameWithIntervals("vns7.mhValue");

    m_sqlstorage->addNameWithIntervals("vns8.cur1Value");
    m_sqlstorage->addNameWithIntervals("vns8.freq1Value");
    m_sqlstorage->addNameWithIntervals("vns8.curPer1Value");
    m_sqlstorage->addNameWithIntervals("vns8.cur2Value");
    m_sqlstorage->addNameWithIntervals("vns8.freq2Value");
    m_sqlstorage->addNameWithIntervals("vns8.curPer2Value");

    m_sqlstorage->addNameWithIntervals("vns1.cur1Value");
    m_sqlstorage->addNameWithIntervals("vns1.freq1Value");
    m_sqlstorage->addNameWithIntervals("vns1.curPer1Value");
    m_sqlstorage->addNameWithIntervals("vns1.cur2Value");
    m_sqlstorage->addNameWithIntervals("vns1.freq2Value");
    m_sqlstorage->addNameWithIntervals("vns1.curPer2Value");

    m_sqlstorage->addNameWithIntervals("_bs.presValue");
    m_sqlstorage->addNameWithIntervals("_bs.mh1Value");
    m_sqlstorage->addNameWithIntervals("_bs.m1Value");

    m_sqlstorage->addNameWithIntervals("kosk.pump1GridValue");
    m_sqlstorage->addNameWithIntervals("kosk.pump2GridValue");
    m_sqlstorage->addNameWithIntervals("kosk.pump3GridValue");
    m_sqlstorage->addNameWithIntervals("kosk.pumpOsadok1Value");
    m_sqlstorage->addNameWithIntervals("kosk.pumpOsadok2Value");
    m_sqlstorage->addNameWithIntervals("kosk.pumpOsadok3Value");
    m_sqlstorage->addNameWithIntervals("kosk.pumpOsadok4Value");
    m_sqlstorage->addNameWithIntervals("kosk.primary1Value");
    m_sqlstorage->addNameWithIntervals("kosk.primary2Value");
    m_sqlstorage->addNameWithIntervals("kosk.primary3Value");
    m_sqlstorage->addNameWithIntervals("kosk.primary4Value");
    m_sqlstorage->addNameWithIntervals("kosk.pump1DrenValue");
    m_sqlstorage->addNameWithIntervals("kosk.pump2DrenValue");
    m_sqlstorage->addNameWithIntervals("kosk.pump3DrenValue");
    m_sqlstorage->addNameWithIntervals("kosk.secondary1Value");
    m_sqlstorage->addNameWithIntervals("kosk.secondary2Value");
    m_sqlstorage->addNameWithIntervals("kosk.secondary3Value");
    m_sqlstorage->addNameWithIntervals("kosk.secondary4Value");

    m_stateStrings.insert("linkState", "Связь");
    m_stateStrings.insert("fireState", "Пожар");
    m_stateStrings.insert("tempState", "Температура");
    m_stateStrings.insert("openState", "Проникновение");
    m_stateStrings.insert("overState", "Затопление");
    m_stateStrings.insert("powerState", "Питание");

    m_sqlstorage->setStationNames(m_stationNames);
}

Server::~Server()
{
    delete m_vnsFatekDefinitions;
    delete m_knsFatekDefinitions;
    delete m_knsModbusDefinitions;
}

void Server::sendTestSMS()
{
    bool res = sendSMS(m_mainWindow.smsForm->ui->numberSMSEdit->text(), m_mainWindow.smsForm->ui->textSMSEdit->text());
    if (res)
        m_mainWindow.smsForm->ui->statusSMSLabel->setText("Статус: ОК");
    else
        m_mainWindow.smsForm->ui->statusSMSLabel->setText("Статус: error");
}

void Server::newVnsRegisterArray(const CSRegistersArray &registerArray)
{
    quint8 stationId = registerArray.stationAddress;
    QString stationName = "";
    for(int i = 0; i < m_vnsFatekDefinitions->size(); ++i) {
        if (m_vnsFatekDefinitions->at(i).stationId == stationId) {
            stationName = m_vnsFatekDefinitions->at(i).name;
            break;
        }
    }

    if (stationName.isEmpty()) {
        m_mainWindow.setLogLine(QString("Получены регистры от станции с неизвестным stationId: %1").arg(stationId), MainWindow::LogTarget::ALL);
        return;
    }

    if (stationName == "vns1"){
        QString vns1Name = stationName;
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(vns1Name, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(vns1Name, "linkState", "0")){
                m_sqlstorage->archiveRecord(vns1Name, "linkState", "0");
                m_sqlstorage->archiveRecord(vns1Name, "mess", m_stationNames[vns1Name]+": связь восстановлена;#60FF60", ToStorage);
            }
        }

        if (registerArray.isWithRegister(0)){
            quint16 R0 = registerArray.getRegister(0);
            m_sqlstorage->archiveTristate(vns1Name, "powerState", (R0 & 0x1)?1:0, "Питание");
            m_sqlstorage->archiveTristate(vns1Name, "openState", (R0 & 0x2)?1:0, "Проникновение");
            m_sqlstorage->archiveTristate(vns1Name, "fireState", (R0 & 0x4)?1:0, "Пожар");
            m_sqlstorage->archiveTristate(vns1Name, "tempState", (R0 & 0x8)?1:0, "Температура");
            m_sqlstorage->archiveTristate(vns1Name, "overState", (R0 & 0x10)?1:0, "Затопление");
            m_sqlstorage->archiveRecord(vns1Name, "pump1State", (R0 & 0x20)?1:0, OnChange | ToStorage);
            m_sqlstorage->archiveRecord(vns1Name, "pump2State", (R0 & 0x40)?1:0, OnChange | ToStorage);
            m_sqlstorage->archiveRecord(vns1Name, "pump3State", (R0 & 0x80)?1:0, OnChange | ToStorage);
            m_sqlstorage->archiveRecord(vns1Name, "pump4State", (R0 & 0x100)?1:0, OnChange | ToStorage);
            m_sqlstorage->archiveTristate(vns1Name, "linkPC1State", (R0 & 0x1000)?0:1, "Связь ПЧ1");
            m_sqlstorage->archiveTristate(vns1Name, "errorPC1State", (R0 & 0x2000)?0:1, "Ошибка ПЧ1");
            m_sqlstorage->archiveTristate(vns1Name, "linkPC2State", (R0 & 0x4000)?0:1, "Связь ПЧ2");
            m_sqlstorage->archiveTristate(vns1Name, "errorPC2State", (R0 & 0x8000)?0:1, "Ошибка ПЧ2");
        }
        if (registerArray.isWithRegister(1) && registerArray.isWithRegister(2)){
            //quint32 R1 = registerArray.getRegister(1);
            quint32 R2 = registerArray.getRegister(2);
            double presOut = R2; //R1 + R2 * 0x10000;
            presOut = presOut / 10;
            m_sqlstorage->archiveRecord(vns1Name, "presOutValue", presOut, ToStorage);
        }
        if (registerArray.isWithRegister(3)){
            double level = registerArray.getRegister(3);
            level = level / 10 / 4.3 * 100;
            m_sqlstorage->archiveRecord(vns1Name, "waterLevelValue", level, ToStorage);
        }
        static int mh1Value = 0, mh2Value = 0;
        static int m1Value = 0, m2Value = 0;
        if (registerArray.isWithRegister(4) && registerArray.isWithRegister(5)){
            quint32 R4 = registerArray.getRegister(4);
            quint32 R5 = registerArray.getRegister(5);
            int inning1 = R4 + R5 * 0x10000;
            mh1Value = inning1;
            m_sqlstorage->archiveRecord(vns1Name, "mh1Value", inning1, ToStorage);
            m_sqlstorage->archiveRecord(vns1Name, "mhsValue", mh1Value+mh2Value, ToStorage);
        }
        if (registerArray.isWithRegister(6) && registerArray.isWithRegister(7)){
            quint32 R6 = registerArray.getRegister(6);
            quint32 R7 = registerArray.getRegister(7);
            int consumption = R6 + R7 * 0x10000;
            m1Value = consumption;
            m_sqlstorage->archiveRecord(vns1Name, "m1Value", consumption, ToStorage);
            m_sqlstorage->archiveRecord(vns1Name, "msValue", m1Value+m2Value, ToStorage);
        }
        if (registerArray.isWithRegister(8) && registerArray.isWithRegister(9)){
            quint32 R8 = registerArray.getRegister(8);
            quint32 R9 = registerArray.getRegister(9);
            int inning2 = R8 + R9 * 0x10000;
            mh2Value = inning2;
            m_sqlstorage->archiveRecord(vns1Name, "mh2Value", inning2, ToStorage);
            m_sqlstorage->archiveRecord(vns1Name, "mhsValue", mh1Value+mh2Value, ToStorage);
        }
        if (registerArray.isWithRegister(10) && registerArray.isWithRegister(11)){
            quint32 R10 = registerArray.getRegister(10);
            quint32 R11 = registerArray.getRegister(11);
            int consumption = R10 + R11 * 0x10000;
            m2Value = consumption;
            m_sqlstorage->archiveRecord(vns1Name, "m2Value", consumption, ToStorage);
            m_sqlstorage->archiveRecord(vns1Name, "msValue", m1Value+m2Value, ToStorage);
        }
        if (registerArray.isWithRegister(12) && registerArray.isWithRegister(13)){
            quint32 R12 = registerArray.getRegister(12);
            quint32 R13 = registerArray.getRegister(13);
            double frenq = R12 * 0x100 + R13;
            frenq = frenq / 10;
            m_sqlstorage->archiveRecord(vns1Name, "freq1Value", frenq, ToStorage);
        }
        if (registerArray.isWithRegister(14) && registerArray.isWithRegister(15)){
            quint32 R14 = registerArray.getRegister(14);
            quint32 R15 = registerArray.getRegister(15);
            double cur = R14 * 0x100 + R15;
            cur = cur / 10;
            m_sqlstorage->archiveRecord(vns1Name, "cur1Value", cur, ToStorage);
            m_sqlstorage->archiveRecord(vns1Name, "curPer1Value", cur/716*100, ToStorage);
        }
        if (registerArray.isWithRegister(16) && registerArray.isWithRegister(17)){
            quint32 R16 = registerArray.getRegister(16);
            quint32 R17 = registerArray.getRegister(17);
            double frenq = R16 * 0x100 + R17;
            frenq = frenq / 10;
            m_sqlstorage->archiveRecord(vns1Name, "freq2Value", frenq, ToStorage);
        }
        if (registerArray.isWithRegister(18) && registerArray.isWithRegister(19)){
            quint32 R18 = registerArray.getRegister(18);
            quint32 R19 = registerArray.getRegister(19);
            double cur = R18 * 0x100 + R19;
            cur = cur / 10;
            m_sqlstorage->archiveRecord(vns1Name, "cur2Value", cur, ToStorage);
            m_sqlstorage->archiveRecord(vns1Name, "curPer2Value", cur/716*100, ToStorage);
        }
        if (registerArray.isWithRegister(20) && registerArray.isWithRegister(21)){
            quint32 R20 = registerArray.getRegister(20);
            quint32 R21 = registerArray.getRegister(21);
            int moto = R20 + R21 * 0x10000;
            m_sqlstorage->archiveRecord(vns1Name, "pump1Value", moto, ToStorage);
        }
        if (registerArray.isWithRegister(22) && registerArray.isWithRegister(23)){
            quint32 R22 = registerArray.getRegister(22);
            quint32 R23 = registerArray.getRegister(23);
            int moto = R22 + R23 * 0x10000;
            m_sqlstorage->archiveRecord(vns1Name, "pump2Value", moto, ToStorage);
        }
        if (registerArray.isWithRegister(24) && registerArray.isWithRegister(25)){
            quint32 R24 = registerArray.getRegister(24);
            quint32 R25 = registerArray.getRegister(25);
            int moto = R24 + R25 * 0x10000;
            m_sqlstorage->archiveRecord(vns1Name, "pump3Value", moto, ToStorage);
        }
        if (registerArray.isWithRegister(26) && registerArray.isWithRegister(27)){
            quint32 R26 = registerArray.getRegister(26);
            quint32 R27 = registerArray.getRegister(27);
            int moto = R26 + R27 * 0x10000;
            m_sqlstorage->archiveRecord(vns1Name, "pump4Value", moto, ToStorage);
        }
        return;
    } else if (stationName == "vns5") {
        QString vns5Name = stationName;
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(vns5Name, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(vns5Name, "linkState", "0")){
                m_sqlstorage->archiveRecord(vns5Name, "linkState", "0");
                m_sqlstorage->archiveRecord(vns5Name, "mess", m_stationNames[vns5Name]+": связь восстановлена;#60FF60", ToStorage);
            }
        }

        {
            //Дискретные датчики
            int R = 0;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                //Состояния
                m_sqlstorage->archiveTristate(vns5Name, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(vns5Name, "openState", !(value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(vns5Name, "fireState", !(value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(vns5Name, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(vns5Name, "overState", (value & 0x20), "Затопление");
                //m_sqlstorage->archiveTristate(vns5Name, "highState", (value & 0x40), "Высокий уровень");
                //m_sqlstorage->archiveTristate(vns5Name, "lowState", (value & 0x80), "Низкий уровень");
                m_sqlstorage->archiveTristate(vns5Name, "powerpoint1State", (value & 0x40), "Электроввод №1");
                m_sqlstorage->archiveTristate(vns5Name, "powerpoint2State", (value & 0x80), "Электроввод №2");
                //Состояние насосов
                m_sqlstorage->archiveRecord(vns5Name, "pump1State", (value & 0x100)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(vns5Name, "pump2State", (value & 0x200)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(vns5Name, "pump3State", (value & 0x400)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(vns5Name, "pump4State", (value & 0x800)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(vns5Name, "pump5State", (value & 0x1000)?1:0, OnChange | ToStorage);
                // ПЧ
                m_sqlstorage->archiveTristate(vns5Name, "linkPC", (value & 0x2000)?0:1, "Связь с ПЧ");
                m_sqlstorage->archiveTristate(vns5Name, "errorPC", (value & 0x4000)?1:0, "Ошибка ПЧ");
            }
        }

        {
            //Частота ПЧ
            int R1(1), R2(2), R3(3);
            if (registerArray.isWithRegister(R1) && registerArray.isWithRegister(R2) && registerArray.isWithRegister(R3)) {
                Word16 w;
                w.w = registerArray.getRegister(R1);
                QString strValue(QChar(w.wl));
                w.w = registerArray.getRegister(R2);
                strValue.append(QChar(w.wl));
                strValue.append('.');
                w.w = registerArray.getRegister(R3);
                strValue.append(QChar(w.wl));

                bool parsed = false;
                float freqValue = strValue.toFloat(&parsed);
                if (parsed) {
                    m_sqlstorage->archiveRecord(vns5Name, "freqValue", strValue, ToStorage);
                }
            }
        }

        {
            //Ток ПЧ
            int R1(5), R2(6), R3(7);
            if (registerArray.isWithRegister(R1) && registerArray.isWithRegister(R2) && registerArray.isWithRegister(R3)) {
                Word16 w;
                w.w = registerArray.getRegister(R1);
                QString strValue(QChar(w.wl));
                w.w = registerArray.getRegister(R2);
                strValue.append(QChar(w.wl));
                strValue.append('.');
                w.w = registerArray.getRegister(R3);
                strValue.append(QChar(w.wl));

                bool parsed = false;
                float curValue = strValue.toFloat(&parsed);
                if (parsed) {
                    m_sqlstorage->archiveRecord(vns5Name, "curValue", strValue, ToStorage);
                    float curPerValue = curValue * 100 / 57;
                    m_sqlstorage->archiveRecord(vns5Name, "curPerValue", curPerValue, ToStorage);
                }
            }
        }

        {
            //Моточасы 1 насос
            int R = 14;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns5Name, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 25;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns5Name, "pump2Value", value, ToStorage);
            }
        }
        {
            //Моточасы 3 насос
            int R = 26;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns5Name, "pump3Value", value, ToStorage);
            }
        }
        {
            //Моточасы 4 насос
            int R = 27;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns5Name, "pump4Value", value, ToStorage);
            }
        }
        {
            //Моточасы 5 насос
            int R = 28;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns5Name, "pump5Value", value, ToStorage);
            }
        }

        {
            //Расход V1, м3
            int RH = 16;
            int RL = 17;
            if (registerArray.isWithRegister(RH) && registerArray.isWithRegister(RL)){
                Word16 w1, w2;
                w1.w = registerArray.getRegister(RH);
                w2.w = registerArray.getRegister(RL);
                qint32 val0 = w1.wl;
                qint32 val1 = w1.wh;
                qint32 val2 = w2.wl;
                qint32 val3 = w2.wh;
                int value = val0 | (val1 << 8) | (val2 << 16) | (val3 << 24);
                m_sqlstorage->archiveRecord(vns5Name, "m1Value", value, OnChange | ToStorage);
            }
        }
        {
            //Расход V2, м3
            int RH = 19;
            int RL = 20;
            if (registerArray.isWithRegister(RH) && registerArray.isWithRegister(RL)){
                Word16 w1, w2;
                w1.w = registerArray.getRegister(RH);
                w2.w = registerArray.getRegister(RL);
                qint32 val0 = w1.wl;
                qint32 val1 = w1.wh;
                qint32 val2 = w2.wl;
                qint32 val3 = w2.wh;
                int value = val0 | (val1 << 8) | (val2 << 16) | (val3 << 24);
                m_sqlstorage->archiveRecord(vns5Name, "m2Value", value, OnChange | ToStorage);
            }
        }

        {
            //Подача Q1 м3/ч
            int R = 15;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns5Name, "mh1Value", value, OnChange | ToStorage);
            }
        }
        {
            //Подача Q2 м3/ч
            int R = 18;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns5Name, "mh2Value", value, OnChange | ToStorage);
            }
        }

        {
            //Давление входное
            int R = 13;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns5Name, "presInValue", value/100, ToStorage);
            }
        }

        {
            //Давление выходное
            int R1(9), R2(10);
            if (registerArray.isWithRegister(R1) && registerArray.isWithRegister(R2)) {
                Word16 w;
                w.w = registerArray.getRegister(R1);
                QString strValue(QChar(w.wl));
                strValue.append('.');
                w.w = registerArray.getRegister(R2);
                strValue.append(QChar(w.wl));

                bool parsed = false;
                float presOutValue = strValue.toFloat(&parsed);
                if (parsed) {
                    m_sqlstorage->archiveRecord(vns5Name, "presOutValue", strValue, ToStorage);
                }
            }
        }
        return;
    } else if (stationName == "vns2") {
        QString vns2Name = stationName;
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(vns2Name, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(vns2Name, "linkState", "0")){
                m_sqlstorage->archiveRecord(vns2Name, "linkState", "0");
                m_sqlstorage->archiveRecord(vns2Name, "mess", m_stationNames[vns2Name]+": связь восстановлена;#60FF60", ToStorage);
            }
        }

        {
            //Дискретные датчики
            int R = 0;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                //Состояния
                m_sqlstorage->archiveTristate(vns2Name, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(vns2Name, "openState", !(value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(vns2Name, "fireState", !(value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(vns2Name, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(vns2Name, "overState", (value & 0x20), "Затопление");
                //m_sqlstorage->archiveTristate(vns5Name, "highState", (value & 0x40), "Высокий уровень");
                //m_sqlstorage->archiveTristate(vns5Name, "lowState", (value & 0x80), "Низкий уровень");
                m_sqlstorage->archiveTristate(vns2Name, "powerpoint1State", !(value & 0x100), "Электроввод №1");
                m_sqlstorage->archiveTristate(vns2Name, "powerpoint2State", !(value & 0x80), "Электроввод №2");
                //Состояние насосов
                int n1 = (value & 0x200)?1:0;
                int n2 = (value & 0x400)?1:0;
                int n3 = (value & 0x800)?1:0;
                int n4 = (value & 0x1000)?1:0;
                int n5 = (value & 0x2000)?1:0;
                m_sqlstorage->archiveRecord(vns2Name, "pump1State", n1, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(vns2Name, "pump2State", n2, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(vns2Name, "pump3State", n3, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(vns2Name, "pump4State", n4, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(vns2Name, "pump5State", n5, OnChange | ToStorage);

                if (n1 | n2 | n3 | n4 | n5){
                    m_sqlstorage->archiveRecord(vns2Name, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(vns2Name, "noneState", "0")){
                        m_sqlstorage->archiveRecord(vns2Name, "noneState", "0");
                        m_sqlstorage->archiveRecord(vns2Name, "mess", m_stationNames[vns2Name]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(vns2Name, "noneSignal", "0", OnChange);
                }
                // ПЧ
                m_sqlstorage->archiveTristate(vns2Name, "linkPCState", (value & 0x4000), "Связь с ПЧ");
                m_sqlstorage->archiveTristate(vns2Name, "errorPCState", (value & 0x8000), "Ошибка ПЧ");
            }
        }

        {
            //Частота ПЧ
            int R1(1), R2(2), R3(3);
            if (registerArray.isWithRegister(R1) && registerArray.isWithRegister(R2) && registerArray.isWithRegister(R3)) {
                Word16 w;
                w.w = registerArray.getRegister(R1);
                QString strValue(QChar(w.wl));
                w.w = registerArray.getRegister(R2);
                strValue.append(QChar(w.wl));
                strValue.append('.');
                w.w = registerArray.getRegister(R3);
                strValue.append(QChar(w.wl));

                bool parsed = false;
                float freqValue = strValue.toFloat(&parsed);
                if (parsed) {
                    m_sqlstorage->archiveRecord(vns2Name, "freqValue", strValue, ToStorage);
                }
            }
        }

        {
            //Ток ПЧ
            int R1(5), R2(6), R3(7);
            if (registerArray.isWithRegister(R1) && registerArray.isWithRegister(R2) && registerArray.isWithRegister(R3)) {
                Word16 w;
                w.w = registerArray.getRegister(R1);
                QString strValue(QChar(w.wl));
                w.w = registerArray.getRegister(R2);
                strValue.append(QChar(w.wl));
                strValue.append('.');
                w.w = registerArray.getRegister(R3);
                strValue.append(QChar(w.wl));

                bool parsed = false;
                float curValue = strValue.toFloat(&parsed);
                if (parsed) {
                    m_sqlstorage->archiveRecord(vns2Name, "curValue", strValue, ToStorage);
                    float curPerValue = curValue * 100 / 160;
                    m_sqlstorage->archiveRecord(vns2Name, "curPerValue", curPerValue, ToStorage);
                }
            }
        }

        {
            //Моточасы 1 насос
            int R = 14;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns2Name, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 25;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns2Name, "pump2Value", value, ToStorage);
            }
        }
        {
            //Моточасы 3 насос
            int R = 26;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns2Name, "pump3Value", value, ToStorage);
            }
        }
        {
            //Моточасы 4 насос
            int R = 27;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns2Name, "pump4Value", value, ToStorage);
            }
        }
        {
            //Моточасы 5 насос
            int R = 28;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns2Name, "pump5Value", value, ToStorage);
            }
        }

        {
            //Расход V1, м3
            int RH = 16;
            int RL = 17;
            if (registerArray.isWithRegister(RH) && registerArray.isWithRegister(RL)){
                Word16 w1, w2;
                w1.w = registerArray.getRegister(RH);
                w2.w = registerArray.getRegister(RL);
                qint32 val0 = w1.wl;
                qint32 val1 = w1.wh;
                qint32 val2 = w2.wl;
                qint32 val3 = w2.wh;
                int value = val0 | (val1 << 8) | (val2 << 16) | (val3 << 24);
                m_sqlstorage->archiveRecord(vns2Name, "m1Value", value, OnChange | ToStorage);
            }
        }
        {
            //Расход V2, м3
            int RH = 19;
            int RL = 20;
            if (registerArray.isWithRegister(RH) && registerArray.isWithRegister(RL)){
                Word16 w1, w2;
                w1.w = registerArray.getRegister(RH);
                w2.w = registerArray.getRegister(RL);
                qint32 val0 = w1.wl;
                qint32 val1 = w1.wh;
                qint32 val2 = w2.wl;
                qint32 val3 = w2.wh;
                int value = val0 | (val1 << 8) | (val2 << 16) | (val3 << 24);
                m_sqlstorage->archiveRecord(vns2Name, "m2Value", value, OnChange | ToStorage);
            }
        }

        {
            //Подача Q1 м3/ч
            int R = 15;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns2Name, "mh1Value", value, OnChange | ToStorage);
            }
        }
        {
            //Подача Q2 м3/ч
            int R = 18;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns2Name, "mh2Value", value, OnChange | ToStorage);
            }
        }

        {
            //Давление входное
            int R = 13;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns2Name, "presInValue", value/100, ToStorage);
            }
        }

        {
            //Давление выходное
            int R1(9), R2(10);
            if (registerArray.isWithRegister(R1) && registerArray.isWithRegister(R2)) {
                Word16 w;
                w.w = registerArray.getRegister(R1);
                QString strValue(QChar(w.wl));
                strValue.append('.');
                w.w = registerArray.getRegister(R2);
                strValue.append(QChar(w.wl));

                bool parsed = false;
                float presOutValue = strValue.toFloat(&parsed);
                if (parsed) {
                    m_sqlstorage->archiveRecord(vns2Name, "presOutValue", strValue, ToStorage);
                }
            }
        }

        {
            int R = 29;
            if (registerArray.isWithRegister(R)) {
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns2Name, "waterLevel1Value", value, ToStorage);
            }
        }

        {
            int R = 30;
            if (registerArray.isWithRegister(R)) {
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns2Name, "waterLevel2Value", value, ToStorage);
            }
        }
        return;
    } else if (stationName == "vns6") {
        QString vns6Name = stationName;
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(vns6Name, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(vns6Name, "linkState", "0")){
                m_sqlstorage->archiveRecord(vns6Name, "linkState", "0");
                m_sqlstorage->archiveRecord(vns6Name, "mess", m_stationNames[vns6Name]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            // Частота
            int R = 1;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);;
                value = value / 10;
                m_sqlstorage->archiveRecord(vns6Name, "freqValue", value, ToStorage);
            }
        }
        {
            // Частота 2 ПЧ
            int R = 2;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);;
                value = value / 10;
                m_sqlstorage->archiveRecord(vns6Name, "freqValue2", value, ToStorage);
            }
        }
        {
            //Ток
            int R = 5;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns6Name, "curValue", value, ToStorage);
            }
            //% Тока от номинала
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value /= 4.80;
                m_sqlstorage->archiveRecord(vns6Name, "curPerValue", value, ToStorage);
            }
        }
        {
            //Ток 2
            int R = 6;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 10;
                m_sqlstorage->archiveRecord(vns6Name, "curValue2", value , ToStorage);
            }
        }
        {
            //Давление входное
            int R = 13;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
               // value = value / 87.8;
                value = value / 100;
                m_sqlstorage->archiveRecord(vns6Name, "presInValue", value, ToStorage);
            }
        }
        {
            //Давление выходное
            int R = 9;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);;
                value = value / 100;
                m_sqlstorage->archiveRecord(vns6Name, "presOutValue", value, ToStorage);
            }
        }
        {
            //Давление выходное 2
            int R = 10;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);;
                value = value / 10;
                m_sqlstorage->archiveRecord(vns6Name, "presOutValue2", value, ToStorage);
            }
        }
        {
            //Давление контрольное
            int R = 7;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);;
                value = value / 100;
                m_sqlstorage->archiveRecord(vns6Name, "controlPresOutValue", value, ToStorage);
            }
        }
        {
            //Установленное давление
            int R = 35;
            if (registerArray.isWithRegister((R))) {
                double value = registerArray.getRegister(R);
                value = value / 1638.4;
                m_sqlstorage->archiveRecord(vns6Name, "presValueSet", value, ToStorage);
            }
        }
        /*{
            //Уровень сигнала
            int R = 0;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                value = value * 200 / 112;
                m_sqlstorage->archiveRecord(vns6Name, "levelValue", value, ToStorage);
            }
        }*/
        {
            //Моточасы 1 насос
            int R = 14;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns6Name, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 25;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns6Name, "pump2Value", value, ToStorage);
            }
        }
        {
            //Моточасы 3 насос
            int R = 26;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns6Name, "pump3Value", value, ToStorage);
            }
        }
        {
            //Моточасы 4 насос
            int R = 27;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns6Name, "pump4Value", value, ToStorage);
            }
        }
        {
            //Моточасы 5 насос
            int R = 28;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns6Name, "pump5Value", value, ToStorage);
            }
        }
        {
            //Дискретные датчики
            int R = 0;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveTristate(vns6Name, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(vns6Name, "openState", !(value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(vns6Name, "fireState", !(value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(vns6Name, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(vns6Name, "overState", (value & 0x20), "Затопление");
                //m_sqlstorage->archiveTristate(vns6Name, "highState", (value & 0x40), "Высокий уровень");
                //m_sqlstorage->archiveTristate(station, "lowState", (value & 0x80), "Низкий уровень");
                m_sqlstorage->archiveTristate(vns6Name, "powerpoint1State", !(value & 0x80), "Электроввод №1");
                m_sqlstorage->archiveTristate(vns6Name, "powerpoint2State", !(value & 0x100), "Электроввод №2");

                int n1 = (value & 0x200)?1:0;
                int n2 = (value & 0x400)?1:0;
                int n3 = (value & 0x800)?1:0;
                int n4 = (value & 0x1000)?1:0;
                int n5 = (value & 0x2000)?1:0;
                m_sqlstorage->archiveRecord(vns6Name, "pump1State", n1, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(vns6Name, "pump2State", n2, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(vns6Name, "pump3State", n3, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(vns6Name, "pump4State", n4, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(vns6Name, "pump5State", n5, OnChange | ToStorage);

                if (n1 | n2 | n3 | n4 | n5){
                    m_sqlstorage->archiveRecord(vns6Name, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(vns6Name, "noneState", "0")){
                        m_sqlstorage->archiveRecord(vns6Name, "noneState", "0");
                        m_sqlstorage->archiveRecord(vns6Name, "mess", m_stationNames[vns6Name]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(vns6Name, "noneSignal", "0", OnChange);
                }

                m_sqlstorage->archiveTristate(vns6Name, "linkPCState", (value & 0x4000), "Связь с ПЧ");
                m_sqlstorage->archiveTristate(vns6Name, "errorPCState", (value & 0x8000), "Ошибка ПЧ");
            }
        }

        {
            //Подача Q1 м3/ч
            int R = 18;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns6Name, "mh1Value", value, OnChange | ToStorage);
            }
        }
        {
            //Подача Q2 м3/ч
            int R = 32;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns6Name, "mh2Value", value, OnChange | ToStorage);
            }
        }
        {
            //Расход V1, м3
            int RH = 19;
            int RL = 20;
            if (registerArray.isWithRegister(RH) && registerArray.isWithRegister(RL)){
                Word16 w1, w2;
                w1.w = registerArray.getRegister(RH);
                w2.w = registerArray.getRegister(RL);
                qint32 val0 = w1.wl;
                qint32 val1 = w1.wh;
                qint32 val2 = w2.wl;
                qint32 val3 = w2.wh;
                int value = val0 | (val1 << 8) | (val2 << 16) | (val3 << 24);
                m_sqlstorage->archiveRecord(vns6Name, "m1Value", value, OnChange | ToStorage);
            }
        }
        {
            //Расход V2, м3
            int RH = 33;
            int RL = 34;
            if (registerArray.isWithRegister(RH) && registerArray.isWithRegister(RL)){
                Word16 w1, w2;
                w1.w = registerArray.getRegister(RH);
                w2.w = registerArray.getRegister(RL);
                qint32 val0 = w1.wl;
                qint32 val1 = w1.wh;
                qint32 val2 = w2.wl;
                qint32 val3 = w2.wh;
                int value = val0 | (val1 << 8) | (val2 << 16) | (val3 << 24);
                m_sqlstorage->archiveRecord(vns6Name, "m2Value", value, OnChange | ToStorage);
            }
        }

        {
            //Подача QIn м3/ч
            int R = 15;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns6Name, "mhInValue", value, OnChange | ToStorage);
            }
        }
        {
            //Объем Vin, м3
            int RH = 16;
            int RL = 17;
            if (registerArray.isWithRegister(RH) && registerArray.isWithRegister(RL)){
                Word16 w1, w2;
                w1.w = registerArray.getRegister(RH);
                w2.w = registerArray.getRegister(RL);
                qint32 val0 = w1.wl;
                qint32 val1 = w1.wh;
                qint32 val2 = w2.wl;
                qint32 val3 = w2.wh;
                int value = val0 | (val1 << 8) | (val2 << 16) | (val3 << 24);
                m_sqlstorage->archiveRecord(vns6Name, "mInValue", value, OnChange | ToStorage);
            }
        }

        {
            int R = 29;
            if (registerArray.isWithRegister(R)) {
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns6Name, "waterLevel1Value", value, ToStorage);
            }
        }

        {
            int R = 30;
            if (registerArray.isWithRegister(R)) {
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns6Name, "waterLevel2Value", value, ToStorage);
            }
        }
        { // температура ПЧ 1
            int R = 38;
            if (registerArray.isWithRegister(R)) {
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns6Name, "temp", value, ToStorage);
            }
        }
        { // температура ПЧ 2
            int R = 39;
            if (registerArray.isWithRegister(R)) {
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns6Name, "temp2", value, ToStorage);
            }
        }
        { // мощность ПЧ 2
            int R = 4;
            if (registerArray.isWithRegister(R)) {
                int value = registerArray.getRegister(R);;
                m_sqlstorage->archiveRecord(vns6Name, "energy2", value, ToStorage);
            }
        }

        return;
    } else if (stationName == "vns4") {
        QString vns4Name = stationName;
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(vns4Name, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(vns4Name, "linkState", "0")){
                m_sqlstorage->archiveRecord(vns4Name, "linkState", "0");
                m_sqlstorage->archiveRecord(vns4Name, "mess", m_stationNames[vns4Name]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            //Подача Q м3/ч
            int R = 15;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns4Name, "mh1Value", value, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(vns4Name, "mhsValue", value, OnChange | ToStorage);
            }
        }
        {
            // Частота
            int R = 1;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 10;
                m_sqlstorage->archiveRecord(vns4Name, "freqValue", value, OnChange | ToStorage);
            }
        }
        {
            //Ток
            int R = 5;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 100;
                m_sqlstorage->archiveRecord(vns4Name, "curValue", value, OnChange | ToStorage);
            }
            //% Тока от номинала
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 160;
                m_sqlstorage->archiveRecord(vns4Name, "curPerValue", value, OnChange | ToStorage);
            }
        }
        {
            //Давление входное
            int R = 13;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 114.8 * 10;
                m_sqlstorage->archiveRecord(vns4Name, "presInValue", value, OnChange | ToStorage);
            }
        }
        {
            //Давление выходное
            int R = 9;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 11480;
                m_sqlstorage->archiveRecord(vns4Name, "presOutValue", value, OnChange | ToStorage);
            }
        }
        {
            //Расход V, м3
            int RH = 16;
            int RL = 17;
            if (registerArray.isWithRegister(RH) && registerArray.isWithRegister(RL)){
                Word16 w1, w2;
                w1.w = registerArray.getRegister(RH);
                w2.w = registerArray.getRegister(RL);
                qint32 val0 = w1.wl;
                qint32 val1 = w1.wh;
                qint32 val2 = w2.wl;
                qint32 val3 = w2.wh;
                int value = val0 | (val1 << 8) | (val2 << 16) | (val3 << 24);
                m_sqlstorage->archiveRecord(vns4Name, "m1Value", value, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(vns4Name, "msValue", value, OnChange | ToStorage);
            }
        }
        {
            //Моточасы 1 насос
            int R = 14;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns4Name, "pump1Value", value, OnChange | ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 25;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns4Name, "pump2Value", value, ToStorage);
            }
        }
        {
            //Моточасы 3 насос
            int R = 26;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns4Name, "pump3Value", value, ToStorage);
            }
        }
        {
            //Моточасы 4 насос
            int R = 27;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns4Name, "pump4Value", value, ToStorage);
            }
        }
        {
            //Дискретные датчики
            int R = 0;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                //Состояния
                m_sqlstorage->archiveTristate(vns4Name, "powerState", (value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(vns4Name, "openState", (value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(vns4Name, "fireState", (value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(vns4Name, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(vns4Name, "overState", (value & 0x20), "Затопление");
                //m_sqlstorage->archiveTristate(station, "highState", (value & 0x40), "Высокий уровень");
                //m_sqlstorage->archiveTristate(station, "lowState", (value & 0x80), "Низкий уровень");
                m_sqlstorage->archiveTristate(vns4Name, "powerpoint1State", (value & 0x40), "Электроввод №1");
                m_sqlstorage->archiveTristate(vns4Name, "powerpoint2State", (value & 0x80), "Электроввод №2");
                //Состояние насосов
                m_sqlstorage->archiveRecord(vns4Name, "pump1State", (value & 0x100)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(vns4Name, "pump2State", (value & 0x200)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(vns4Name, "pump3State", (value & 0x400)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(vns4Name, "pump4State", (value & 0x800)?1:0, OnChange | ToStorage);
                // ПЧ
                //m_sqlstorage->archiveTristate(station, "linkPC", (value & 0x1000)?0:1, "Связь с ПЧ");
                //m_sqlstorage->archiveTristate(station, "errorPC", (value & 0x2000)?1:0, "Ошибка ПЧ");
            }
        }
        return;
    } else if (stationName == "vns3"){
        QString vns3Name = stationName;
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(vns3Name, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(vns3Name, "linkState", "0")){
                m_sqlstorage->archiveRecord(vns3Name, "linkState", "0");
                m_sqlstorage->archiveRecord(vns3Name, "mess", m_stationNames[vns3Name]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            // Частота
            int R = 1;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 10;
                m_sqlstorage->archiveRecord(vns3Name, "freqValue", value, ToStorage);
            }
        }
        {
            //Ток
            int R = 5;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 10;
                m_sqlstorage->archiveRecord(vns3Name, "curValue", value, ToStorage);
            }
            //% Тока от номинала
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value * 100 / 2016;
                m_sqlstorage->archiveRecord(vns3Name, "curPerValue", value, ToStorage);
            }
        }
        {
            //Давление входное
            int R = 13;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 100;
                m_sqlstorage->archiveRecord(vns3Name, "presInValue", value, ToStorage);
            }
        }
        {
            //Давление выходное
            int R = 9;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 1000;
                m_sqlstorage->archiveRecord(vns3Name, "presOutValue", value, ToStorage);
            }
        }
        /*{
            //Уровень сигнала
            int R = 0;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                value = value * 200 / 112;
                m_sqlstorage->archiveRecord(station, "levelValue", value, ToStorage);
            }
        }*/
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(vns3Name, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(vns3Name, "linkState", "0")){
                m_sqlstorage->archiveRecord(vns3Name, "linkState", "0");
                m_sqlstorage->archiveRecord(vns3Name, "mess", m_stationNames[vns3Name]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        static double mh1Value = 0, mh2Value = 0;
        static double m1Value = 0, m2Value = 0;
        {
            //Расход q1
            int R = 15;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns3Name, "mh1Value", value, ToStorage);
                mh1Value = value;
                m_sqlstorage->archiveRecord(vns3Name, "mhsValue", mh1Value+mh2Value, ToStorage);
            }
        }
        {
            //Расход q2
            int R = 18;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns3Name, "mh2Value", value, ToStorage);
                mh2Value = value;
                m_sqlstorage->archiveRecord(vns3Name, "mhsValue", mh1Value+mh2Value, ToStorage);
            }
        }
        {
            //Объем V1
            int RH = 16;
            int RL = 17;
            if (registerArray.isWithRegister(RH) && registerArray.isWithRegister(RL)){
                Word16 w1, w2;
                w1.w = registerArray.getRegister(RH);
                w2.w = registerArray.getRegister(RL);
                qint32 val0 = w1.wl;
                qint32 val1 = w1.wh;
                qint32 val2 = w2.wl;
                qint32 val3 = w2.wh;
                int value = val0 | (val1 << 8) | (val2 << 16) | (val3 << 24);
                m_sqlstorage->archiveRecord(vns3Name, "m1Value", value, ToStorage);
                m1Value = value;
                m_sqlstorage->archiveRecord(vns3Name, "msValue", m1Value+m2Value, ToStorage);
            }
        }
        {
            //Объем V2
            int RH = 19;
            int RL = 20;
            if (registerArray.isWithRegister(RH) && registerArray.isWithRegister(RL)){
                Word16 w1, w2;
                w1.w = registerArray.getRegister(RH);
                w2.w = registerArray.getRegister(RL);
                qint32 val0 = w1.wl;
                qint32 val1 = w1.wh;
                qint32 val2 = w2.wl;
                qint32 val3 = w2.wh;
                int value = val0 | (val1 << 8) | (val2 << 16) | (val3 << 24);
                m_sqlstorage->archiveRecord(vns3Name, "m2Value", value, ToStorage);
                m2Value = value;
                m_sqlstorage->archiveRecord(vns3Name, "msValue", m1Value+m2Value, ToStorage);
            }
        }
        {
            //Уровень
            int R = 29;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value * 100 / 36;
                m_sqlstorage->archiveRecord(vns3Name, "waterLevelValue", (int)value, ToStorage);
            }
        }
        /*{
            //Вводы
            int R = 14;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "input1", (value&0x1)?1:0);
                m_sqlstorage->archiveRecord(station, "input2", (value&0x2)?1:0);
            }
        }*/
        {
            //Моточасы 1 насос
            int R = 14;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns3Name, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 25;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns3Name, "pump2Value", value, ToStorage);
            }
        }
        {
            //Состояния
            int R = 0;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveTristate(vns3Name, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(vns3Name, "over1State", (value & 0x4), "Затопление 1");
                m_sqlstorage->archiveTristate(vns3Name, "tempState", (value & 0x8), "Температура");
                m_sqlstorage->archiveTristate(vns3Name, "fireState", !(value & 0x10), "Пожар");
                m_sqlstorage->archiveTristate(vns3Name, "over2State", !(value & 0x20), "Затопление 2");
                m_sqlstorage->archiveTristate(vns3Name, "handState", (value & 0x40));
                m_sqlstorage->archiveTristate(vns3Name, "autoState", (value & 0x80));

                int n1 = (value & 0x100)?1:0;
                int n2 = (value & 0x200)?1:0;

                m_sqlstorage->archiveRecord(vns3Name, "pump1State", n1?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(vns3Name, "pump2State", n2?1:0, OnChange | ToStorage);

                if (n1 | n2){
                    m_sqlstorage->archiveRecord(vns3Name, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(vns3Name, "noneState", "0")){
                        m_sqlstorage->archiveRecord(vns3Name, "noneState", "0");
                        m_sqlstorage->archiveRecord(vns3Name, "mess", m_stationNames[vns3Name]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(vns3Name, "noneSignal", "0", OnChange);
                }
            }
        }
        return;
    } else if (stationName == "vns7"){
        QString vns7Name = stationName;
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(vns7Name, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(vns7Name, "linkState", "0")){
                m_sqlstorage->archiveRecord(vns7Name, "linkState", "0");
                m_sqlstorage->archiveRecord(vns7Name, "mess", m_stationNames[vns7Name]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            // Частота 1
            int R = 1;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 10;
                m_sqlstorage->archiveRecord(vns7Name, "freq1Value", value, ToStorage);
            }
        }
        {
            //Ток 1
            int R = 2;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 100;
                m_sqlstorage->archiveRecord(vns7Name, "cur1Value", value, ToStorage);
            }
            //% Тока от номинала 1
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 44.0;
                m_sqlstorage->archiveRecord(vns7Name, "curPer1Value", value, ToStorage);
            }
        }
        {
            // Частота 2
            int R = 4;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 10;
                m_sqlstorage->archiveRecord(vns7Name, "freq2Value", value, ToStorage);
            }
        }
        {
            //Ток 2
            int R = 5;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 100;
                m_sqlstorage->archiveRecord(vns7Name, "cur2Value", value, ToStorage);
            }
            //% Тока от номинала 2
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 44.0;
                m_sqlstorage->archiveRecord(vns7Name, "curPer2Value", value, ToStorage);
            }
        }
        {
            // Частота 3
            int R = 7;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 10;
                m_sqlstorage->archiveRecord(vns7Name, "freq3Value", value, ToStorage);
            }
        }
        {
            //Ток 3
            int R = 8;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 100;
                m_sqlstorage->archiveRecord(vns7Name, "cur3Value", value, ToStorage);
            }
            //% Тока от номинала 3
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 44.0;
                m_sqlstorage->archiveRecord(vns7Name, "curPer3Value", value, ToStorage);
            }
        }
        {
            //Давление входное
            int R = 10;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 100;
                m_sqlstorage->archiveRecord(vns7Name, "presInValue", value, ToStorage);
            }
        }
        {
            //Давление выходное
            int R = 11;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 100;
                m_sqlstorage->archiveRecord(vns7Name, "presOutValue", value, ToStorage);
            }
        }
        {
            //Моточасы 1 насос
            int R = 12;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns7Name, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 13;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns7Name, "pump2Value", value, ToStorage);
            }
        }
        {
            //Моточасы 3 насос
            int R = 14;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns7Name, "pump3Value", value, ToStorage);
            }
        }
        {
            //Расход q
            int R = 15;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(vns7Name, "mhValue", value, ToStorage);
            }
        }
        {
            //Объем V
            int RH = 16;
            int RL = 17;
            if (registerArray.isWithRegister(RH) && registerArray.isWithRegister(RL)){
                Word16 w1, w2;
                w1.w = registerArray.getRegister(RH);
                w2.w = registerArray.getRegister(RL);
                qint32 val0 = w1.wl;
                qint32 val1 = w1.wh;
                qint32 val2 = w2.wl;
                qint32 val3 = w2.wh;
                int value = val0 | (val1 << 8) | (val2 << 16) | (val3 << 24);
                m_sqlstorage->archiveRecord(vns7Name, "mValue", value, ToStorage);
            }
        }
        if (registerArray.isWithRegister(0)){
            quint16 R0 = registerArray.getRegister(0);
            m_sqlstorage->archiveTristate(vns7Name, "overState", (R0 & 0x1), "Затопление");
            m_sqlstorage->archiveTristate(vns7Name, "powerState", (R0 & 0x2), "Питание");
            m_sqlstorage->archiveTristate(vns7Name, "openState", (R0 & 0x4), "Проникновение");
            m_sqlstorage->archiveTristate(vns7Name, "fireState", (R0 & 0x8), "Пожар");
            m_sqlstorage->archiveTristate(vns7Name, "tempState", (R0 & 0x10), "Температура");

            m_sqlstorage->archiveTristate(vns7Name, "powerpoint1State", (R0 & 0x20), "Электроввод №1");
            m_sqlstorage->archiveTristate(vns7Name, "powerpoint2State", (R0 & 0x40), "Электроввод №2");

            m_sqlstorage->archiveRecord(vns7Name, "pump1State", (R0 & 0x80), OnChange | ToStorage);
            m_sqlstorage->archiveRecord(vns7Name, "pump2State", (R0 & 0x100), OnChange | ToStorage);
            m_sqlstorage->archiveRecord(vns7Name, "pump3State", (R0 & 0x200), OnChange | ToStorage);

            m_sqlstorage->archiveTristate(vns7Name, "errorPC1State", (R0 & 0x400), "Ошибка ПЧ1");
            m_sqlstorage->archiveTristate(vns7Name, "errorPC2State", (R0 & 0x800), "Ошибка ПЧ2");
            m_sqlstorage->archiveTristate(vns7Name, "errorPC3State", (R0 & 0x1000), "Ошибка ПЧ3");

            m_sqlstorage->archiveTristate(vns7Name, "linkPC1State", (R0 & 0x2000), "Связь ПЧ1");
            m_sqlstorage->archiveTristate(vns7Name, "linkPC2State", (R0 & 0x4000), "Связь ПЧ2");
            m_sqlstorage->archiveTristate(vns7Name, "linkPC3State", (R0 & 0x8000), "Связь ПЧ3");
        }
        return;
    } else if (stationName == "vns8") {
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(stationName, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(stationName, "linkState", "0")){
                m_sqlstorage->archiveRecord(stationName, "linkState", "0");
                m_sqlstorage->archiveRecord(stationName, "mess", m_stationNames[stationName]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            // Частота
            int R = 3;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value /= 10.0;
                m_sqlstorage->archiveRecord(stationName, "freq1Value", value, ToStorage);
            }
        }
        {
            //Ток
            int R = 4;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value /= 100.0;
                m_sqlstorage->archiveRecord(stationName, "cur1Value", value, ToStorage);
            }
            //% Тока от номинала
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 5.0;
                m_sqlstorage->archiveRecord(stationName, "curPer1Value", value, ToStorage);
            }
        }
        {
            // Частота
            int R = 6;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value /= 10.0;
                m_sqlstorage->archiveRecord(stationName, "freq2Value", value, ToStorage);
            }
        }
        {
            //Ток
            int R = 7;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value /= 100.0;
                m_sqlstorage->archiveRecord(stationName, "cur2Value", value, ToStorage);
            }
            //% Тока от номинала
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 5.0;
                m_sqlstorage->archiveRecord(stationName, "curPer2Value", value, ToStorage);
            }
        }
        {
            //Давление входное
            int R = 1;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value /= 100.0;
                m_sqlstorage->archiveRecord(stationName, "presInValue", value, ToStorage);
            }
        }
        {
            //Давление выходное
            int R = 2;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 100.0;
                if (value >= 0 && value <= 8.0) {
                    m_sqlstorage->archiveRecord(stationName, "presOutValue", value, ToStorage);
                }
            }
        }
        {
            //Моточасы 1 насос
            int R = 5;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(stationName, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 8;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(stationName, "pump2Value", value, ToStorage);
            }
        }
        {
            //Дискретные датчики
            int R = 0;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                //Состояния
                m_sqlstorage->archiveTristate(stationName, "overState", (value & MASK_0), "Затопление");
                m_sqlstorage->archiveTristate(stationName, "powerState", (value & MASK_1), "Питание");
                m_sqlstorage->archiveTristate(stationName, "openState", (value & MASK_2), "Вскрытие");
                m_sqlstorage->archiveTristate(stationName, "tempState", (value & MASK_3), "Температура");
                m_sqlstorage->archiveTristate(stationName, "fireState", (value & MASK_4), "Задымление");

                //Состояние насосов
                m_sqlstorage->archiveRecord(stationName, "pump1State", (value & MASK_10), OnChange | ToStorage);
                m_sqlstorage->archiveRecord(stationName, "pump2State", (value & MASK_11), OnChange | ToStorage);

                // ПЧ
                m_sqlstorage->archiveTristate(stationName, "errorPC1", (value & MASK_12), "Ошибка ПЧ1");
                m_sqlstorage->archiveTristate(stationName, "errorPC2", (value & MASK_13), "Ошибка ПЧ2");
                m_sqlstorage->archiveTristate(stationName, "linkPC1", (value & MASK_14), "Связь с ПЧ1");
                m_sqlstorage->archiveTristate(stationName, "linkPC2", (value & MASK_15), "Связь с ПЧ2");
            }
        }
        return;
    } else if (stationName == "_bs") {
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(stationName, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(stationName, "linkState", "0")){
                m_sqlstorage->archiveRecord(stationName, "linkState", "0");
                m_sqlstorage->archiveRecord(stationName, "mess", m_stationNames[stationName]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            //Расход Q1 м3/ч
            int R = 4;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(stationName, "mh1Value", value, OnChange | ToStorage);
            }
        }
        {
            //Объем V1, м3
            int RH = 3;
            int RL = 2;
            if (registerArray.isWithRegister(RH) && registerArray.isWithRegister(RL)){
                int hvalue = registerArray.getRegister(RH);
                int lvalue = registerArray.getRegister(RL);
                int value = lvalue + hvalue*0x10000;
                m_sqlstorage->archiveRecord(stationName, "m1Value", value, OnChange | ToStorage);
            }
        }
        //дополнительный расходомер
        {
            //Расход Q2 м3/ч
            int R = 8;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(stationName, "mh2Value", value, OnChange | ToStorage);
            }
        }
        {
            //Объем V2, м3
            int RH = 6;
            int RL = 7;
            if (registerArray.isWithRegister(RH) && registerArray.isWithRegister(RL)){
                int hvalue = registerArray.getRegister(RH);
                int lvalue = registerArray.getRegister(RL);
                int value = lvalue + hvalue*0x10000;
                m_sqlstorage->archiveRecord(stationName, "m2Value", value, OnChange | ToStorage);
            }
        }
        {
            //Давление входное
            int R = 1;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(stationName, "presValue", value/10, ToStorage);
            }
        }
        {
            //Дискретные датчики
            int R = 0;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                //Состояния
                m_sqlstorage->archiveTristate(stationName, "openState", (value & MASK_1), "Вскрытие");
                m_sqlstorage->archiveTristate(stationName, "tempState", (value & MASK_2), "Температура");
                m_sqlstorage->archiveTristate(stationName, "powerFailureState", (value & MASK_3), "Неполадка питания");
                m_sqlstorage->archiveTristate(stationName, "powerLowState", (value & MASK_4), "Пропадание питания");
            }
        }
        return;
    }
}

void Server::newFatekKnsRegisterArray(const CSRegistersArray &registerArray)
{
    quint8 stationId = registerArray.stationAddress;
    QString station = "";
    for(int i = 0; i < m_knsFatekDefinitions->size(); ++i) {
        if (m_knsFatekDefinitions->at(i).stationId == stationId) {
            station = m_knsFatekDefinitions->at(i).name;
            break;
        }
    }

    if (station.isEmpty()) {
        m_mainWindow.setLogLine(QString("Получены регистры от станции с неизвестным stationId: %1").arg(stationId), MainWindow::LogTarget::ALL);
        return;
    }


    {
        // Время последнего соединения
        QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
        m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

        if (!m_sqlstorage->isSame(station, "linkState", "0")){
            m_sqlstorage->archiveRecord(station, "linkState", "0");
            m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
        }
    }
    {
        //Моточасы 1 насос
//        int R = 2;
//        if (registerArray.isWithRegister(R)){
//            int value = registerArray.getRegister(R);
//            m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
//                  }

        int RH = 2;
        int RL = 3;
        if (registerArray.isWithRegister(RH) && registerArray.isWithRegister(RL)){
            Word16 w1, w2;
            w1.w = registerArray.getRegister(RH);
            w2.w = registerArray.getRegister(RL);
            qint32 val0 = w1.wl;
            qint32 val1 = w1.wh;
            qint32 val2 = w2.wl;
            qint32 val3 = w2.wh;
            int value = val0 | (val1 << 8) | (val2 << 16) | (val3 << 24);
            m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
          }
    }
    if (station != "kns11")
    {
        //Моточасы 2 насос
//        int R = 3;
//        if (registerArray.isWithRegister(R)){
//            int value = registerArray.getRegister(R);
//            m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
//        }

        int RH = 4;
        int RL = 5;
        if (registerArray.isWithRegister(RH) && registerArray.isWithRegister(RL)){
            Word16 w1, w2;
            w1.w = registerArray.getRegister(RH);
            w2.w = registerArray.getRegister(RL);
            qint32 val0 = w1.wl;
            qint32 val1 = w1.wh;
            qint32 val2 = w2.wl;
            qint32 val3 = w2.wh;
            int value = val0 | (val1 << 8) | (val2 << 16) | (val3 << 24);
            m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
        }
    }
    if (station == "kns1" || station == "kns3")
    {
        //Моточасы 3 насос
//        int R = 4;
//        if (registerArray.isWithRegister(R)){
//            int value = registerArray.getRegister(R);
//            m_sqlstorage->archiveRecord(station, "pump3Value", value, ToStorage);
//        }
        int RH = 6;
        int RL = 7;
        if (registerArray.isWithRegister(RH) && registerArray.isWithRegister(RL)){
            Word16 w1, w2;
            w1.w = registerArray.getRegister(RH);
            w2.w = registerArray.getRegister(RL);
            qint32 val0 = w1.wl;
            qint32 val1 = w1.wh;
            qint32 val2 = w2.wl;
            qint32 val3 = w2.wh;
            int value = val0 | (val1 << 8) | (val2 << 16) | (val3 << 24);
            m_sqlstorage->archiveRecord(station, "pump3Value", value, ToStorage);
        }
    }
    if (station == "kns1")
    {
        //Моточасы 4 насос
        int R = 5;
        if (registerArray.isWithRegister(R)){
            int value = registerArray.getRegister(R);
            m_sqlstorage->archiveRecord(station, "pump4Value", value, ToStorage);
        }
    }
    if (station == "kns1")
    {
        //Моточасы 5 насос
        int R = 6;
        if (registerArray.isWithRegister(R)){
            int value = registerArray.getRegister(R);
            m_sqlstorage->archiveRecord(station, "pump5Value", value, ToStorage);
        }
    }
    {
        //Состояние насосов
        int R = 0;
        if (registerArray.isWithRegister(R)) {
            quint16 value = registerArray.getRegister(R);

            int n1 = (value & X16_BIT_MASKS::MASK_11)?1:0;
            m_sqlstorage->archiveRecord(station, "pump1State", n1, OnChange | ToStorage);

            if (station != "kns11") {
                int n2 = (value & X16_BIT_MASKS::MASK_12)?1:0;
                m_sqlstorage->archiveRecord(station, "pump2State", n2, OnChange | ToStorage);
            }

            if (station == "kns1" || station == "kn3") {
                int n3 = (value & X16_BIT_MASKS::MASK_13)?1:0;
                m_sqlstorage->archiveRecord(station, "pump3State", n3, OnChange | ToStorage);
            }

            if (station == "kns1") {
                int n4 = (value & X16_BIT_MASKS::MASK_14)?1:0;
                m_sqlstorage->archiveRecord(station, "pump4State", n4, OnChange | ToStorage);
            }

            if (station == "kns1") {
                int n5 = (value & X16_BIT_MASKS::MASK_15)?1:0;
                m_sqlstorage->archiveRecord(station, "pump5State", n5, OnChange | ToStorage);
            }

            int noneState = value & X16_BIT_MASKS::MASK_9;
            if (noneState){
                m_sqlstorage->archiveRecord(station, "noneSignal", "1", OnChange);
                if (!m_sqlstorage->isSame(station, "noneState", "0")){
                    m_sqlstorage->archiveRecord(station, "noneState", "0");
                    m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                }
            } else {
                m_sqlstorage->archiveRecord(station, "noneSignal", "0", OnChange);
            }

            //m_sqlstorage->archiveTristate(station, "powerpoint1State", (value & X16_BIT_MASKS::MASK_0), "Электроввод №1");
            //m_sqlstorage->archiveTristate(station, "powerpoint2State", (value & X16_BIT_MASKS::MASK_1), "Электроввод №2");
            m_sqlstorage->archiveTristate(station, "powerState", !(value & X16_BIT_MASKS::MASK_2), "Питание");
            m_sqlstorage->archiveTristate(station, "openState", !(value & X16_BIT_MASKS::MASK_3), "Вскрытие");
            m_sqlstorage->archiveTristate(station, "fireState", !(value & X16_BIT_MASKS::MASK_4), "Задымление");
            m_sqlstorage->archiveTristate(station, "tempState", (value & X16_BIT_MASKS::MASK_5), "Температура");
            m_sqlstorage->archiveTristate(station, "overState", (value & X16_BIT_MASKS::MASK_6), "Затопление");
            m_sqlstorage->archiveTristate(station, "highState", (value & X16_BIT_MASKS::MASK_7), "Высокий уровень");
            m_sqlstorage->archiveTristate(station, "lowState", (value & X16_BIT_MASKS::MASK_8), "Низкий уровень");
            m_sqlstorage->archiveTristate(station, "overworkState", (value & X16_BIT_MASKS::MASK_10), "Долгая работа");
        }
    }
    /*{
        int R = 1;
        if (registerArray.isWithRegister(R)) {
            quint16 value = registerArray.getRegister(R);
            m_sqlstorage->archiveTristate(station, "grid1WorkState", (value & X16_BIT_MASKS::MASK_0), "Работа решетки №1");
            m_sqlstorage->archiveTristate(station, "grid2WorkState", (value & X16_BIT_MASKS::MASK_1), "Работа решетки №2");
            m_sqlstorage->archiveTristate(station, "grid3WorkState", (value & X16_BIT_MASKS::MASK_2), "Работа решетки №3");
            m_sqlstorage->archiveTristate(station, "grid1FailState", (value & X16_BIT_MASKS::MASK_3), "Авария решетки №1");
            m_sqlstorage->archiveTristate(station, "grid2FailState", (value & X16_BIT_MASKS::MASK_4), "Авария решетки №2");
            m_sqlstorage->archiveTristate(station, "grid3FailState", (value & X16_BIT_MASKS::MASK_5), "Авария решетки №3");
        }
    }*/
    {
        int R = 8;
        if (registerArray.isWithRegister(R)) {
            int value = registerArray.getRegister(R);
            m_sqlstorage->archiveRecord(station, "pumpOverworkValue", value, ToStorage);
        }
    }
    {
        int R = 9;
        if (registerArray.isWithRegister(R)) {
            int value = registerArray.getRegister(R);
            m_sqlstorage->archiveRecord(station, "pumpInactivityValue", value, ToStorage);
        }
    }
    /*{
        int R = 9;
        if (registerArray.isWithRegister(R)) {
            int value = registerArray.getRegister(R);
            //value = value & 0x00FF;
            m_sqlstorage->archiveRecord(station, "temperatureValue", value, ToStorage);
        }
    }*/

    return;

}

void Server::newModbusKnsRegisterArray(const CSRegistersArray &registerArray)
{
    quint8 stationId = registerArray.stationAddress;
    QString station = "";
    for(int i = 0; i < m_knsModbusDefinitions->size(); ++i) {
        if (m_knsModbusDefinitions->at(i).stationId == stationId) {
            station = m_knsModbusDefinitions->at(i).name;
            break;
        }
    }

    if (station.isEmpty()) {
        m_mainWindow.setLogLine(QString("Получены регистры от станции с неизвестным stationId: %1").arg(stationId), MainWindow::LogTarget::ALL);
        return;
    }

    {
        // Время последнего соединения
        QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
        m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

        if (!m_sqlstorage->isSame(station, "linkState", "0")){
            m_sqlstorage->archiveRecord(station, "linkState", "0");
            m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
        }
    }

    if (station == "kns18") {
        {
            //Моточасы 1 насос
            int R = 1;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 2;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
            }
        }
        {
            //Время бездействия насосов
            int R = 3;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pumpInactivityValue", value, ToStorage);
            }
        }

        {
            //Состояние насосов
            int R = 0;
            if (registerArray.isWithRegister(R)) {
                quint16 value = registerArray.getRegister(R);

                int n1 = (value & X16_BIT_MASKS::MASK_4)?1:0;
                m_sqlstorage->archiveRecord(station, "pump1State", n1, OnChange | ToStorage);


                int n2 = (value & X16_BIT_MASKS::MASK_5)?1:0;
                m_sqlstorage->archiveRecord(station, "pump2State", n2, OnChange | ToStorage);

                int noneState = value & X16_BIT_MASKS::MASK_2;
                if (noneState){
                    m_sqlstorage->archiveRecord(station, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(station, "noneState", "0")){
                        m_sqlstorage->archiveRecord(station, "noneState", "0");
                        m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(station, "noneSignal", "0", OnChange);
                }

                m_sqlstorage->archiveTristate(station, "highState", (value & X16_BIT_MASKS::MASK_0), "Высокий уровень");
                m_sqlstorage->archiveTristate(station, "failState", (value & X16_BIT_MASKS::MASK_1), "Авария станции");
            }

        }
    } else if (station == "kns17") {
        {
            //Моточасы 1 насос
            int R = 2;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 3;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
            }
        }
        {
            //Время бездействия насосов
            int R = 4;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pumpInactivityValue", value, ToStorage);
            }
        }

        {
            //Состояние насосов
            int R = 1;
            if (registerArray.isWithRegister(R)) {
                quint16 value = registerArray.getRegister(R);

                int n1 = (value & X16_BIT_MASKS::MASK_2)?1:0;
                m_sqlstorage->archiveRecord(station, "pump1State", n1, OnChange | ToStorage);


                int n2 = (value & X16_BIT_MASKS::MASK_3)?1:0;
                m_sqlstorage->archiveRecord(station, "pump2State", n2, OnChange | ToStorage);

                int noneState = value & X16_BIT_MASKS::MASK_4;
                if (noneState){
                    m_sqlstorage->archiveRecord(station, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(station, "noneState", "0")){
                        m_sqlstorage->archiveRecord(station, "noneState", "0");
                        m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(station, "noneSignal", "0", OnChange);
                }

                m_sqlstorage->archiveTristate(station, "powerState", (value & X16_BIT_MASKS::MASK_1), "Питание");
                m_sqlstorage->archiveTristate(station, "lowState", (value & X16_BIT_MASKS::MASK_6), "Низкий уровень");
                m_sqlstorage->archiveTristate(station, "highState", (value & X16_BIT_MASKS::MASK_7), "Высокий уровень");
                m_sqlstorage->archiveTristate(station, "failState", (value & X16_BIT_MASKS::MASK_5), "Авария станции");
            }

        }
    }

    return;
}

void Server::setVNS(PumpStationDefinition &def)
{
    QString ipPath = QString("%1/%1_ip").arg(def.name);
    QString portPath = QString("%1/%1_port").arg(def.name);
    QString periodPath = QString("%1/%1_period").arg(def.name);
    QString stationIdPath = QString("%1/%1_stationId").arg(def.name);

    if (!m_settings->contains(ipPath)){
        m_mainWindow.setLogLine(QString("IP адрес %1 не определен").arg(m_stationNames[def.name]), MainWindow::LogTarget::ALL);
        return;
    }

    if (!m_settings->contains(portPath)){
        m_mainWindow.setLogLine(QString("Порт %1 не определен").arg(m_stationNames[def.name]), MainWindow::LogTarget::ALL);
        return;
    }

    if (!m_settings->contains(periodPath)){
        m_mainWindow.setLogLine(QString("Период опроса %1 не определен").arg(m_stationNames[def.name]), MainWindow::LogTarget::ALL);
        return;
    }

    bool hasStationIdSetting = m_settings->contains(stationIdPath);
    if (def.stationId == -1 && !hasStationIdSetting){
        m_mainWindow.setLogLine(QString("Номер станции %1 не определен").arg(m_stationNames[def.name]), MainWindow::LogTarget::ALL);
        return;
    }

    QString ip = m_settings->value(ipPath).toString();
    int port = m_settings->value(portPath).toInt();
    int period = m_settings->value(periodPath).toInt()*1000;
    int stationId = hasStationIdSetting ? m_settings->value(stationIdPath).toInt() : def.stationId;
    if (def.stationId != stationId) {
        def.stationId = stationId;
    }
    CSFatekCollector* fatekCollector = new CSFatekCollector(this);
    fatekCollector->setIpAddress(ip);
    fatekCollector->setPort(port);
    fatekCollector->setRequestPeriod(period);
    fatekCollector->setFirstRegister(def.firstRegister);
    fatekCollector->setRegistersNumber(def.registerCount);
    fatekCollector->addStationId(stationId);
    fatekCollector->setLogFormat(def.name + " -> %1");
    connect(fatekCollector, SIGNAL(newDumpLog(QString)), &m_mainWindow, SLOT(setLogLine(QString)));
    connect(fatekCollector, SIGNAL(newLog(QString)), &m_mainWindow, SLOT(setLogLine(QString)));
    connect(fatekCollector, SIGNAL(newRegistersArray(CSRegistersArray)), SLOT(newVnsRegisterArray(CSRegistersArray)));

    m_vnsFatekCollectors[def.name] = fatekCollector;
    fatekCollector->startRequesting();
}

void Server::setFatekKNS(const QString &name)
{
    QString ipPath = QString("%1/%1_ip").arg(name);
    QString portPath = QString("%1/%1_port").arg(name);
    QString periodPath = QString("%1/%1_period").arg(name);
    QString stationIdPath = QString("%1/%1_stationId").arg(name);

    if (!m_settings->contains(ipPath)){
        m_mainWindow.setLogLine(QString("IP адрес %1 не определен").arg(m_stationNames[name]), MainWindow::LogTarget::ALL);
        return;
    }

    if (!m_settings->contains(portPath)){
        m_mainWindow.setLogLine(QString("Порт %1 не определен").arg(m_stationNames[name]), MainWindow::LogTarget::ALL);
        return;
    }

    if (!m_settings->contains(periodPath)){
        m_mainWindow.setLogLine(QString("Период опроса %1 не определен").arg(m_stationNames[name]), MainWindow::LogTarget::ALL);
        return;
    }

    if (!m_settings->contains(stationIdPath)){
        m_mainWindow.setLogLine(QString("Номер станции %1 не определен").arg(m_stationNames[name]), MainWindow::LogTarget::ALL);
        return;
    }

    QString ip = m_settings->value(ipPath).toString();
    int port = m_settings->value(portPath).toInt();
    int period = m_settings->value(periodPath).toInt()*1000;
    int stationId = m_settings->value(stationIdPath).toInt();

    PumpStationDefinition def;
    def = {name, stationId, 0, 12};
    m_knsFatekDefinitions->push_back(def);

    CSFatekCollector* fatekCollector = new CSFatekCollector(this);
    fatekCollector->setIpAddress(ip);
    fatekCollector->setPort(port);
    fatekCollector->setRequestPeriod(period);
    fatekCollector->setFirstRegister(def.firstRegister);
    fatekCollector->setRegistersNumber(def.registerCount);
    fatekCollector->addStationId(def.stationId);
    fatekCollector->setLogFormat(def.name + " -> %1");
    connect(fatekCollector, SIGNAL(newDumpLog(QString)), &m_mainWindow, SLOT(setLogLine(QString)));
    connect(fatekCollector, SIGNAL(newLog(QString)), &m_mainWindow, SLOT(setLogLine(QString)));
    connect(fatekCollector, SIGNAL(newRegistersArray(CSRegistersArray)), SLOT(newFatekKnsRegisterArray(CSRegistersArray)));

    m_knsFatekCollectors[def.name] = fatekCollector;
    fatekCollector->startRequesting();
}

void Server::setModbusKNS(const QString &name)
{
    QString ipPath = QString("%1/%1_ip").arg(name);
    QString portPath = QString("%1/%1_port").arg(name);
    QString periodPath = QString("%1/%1_period").arg(name);
    QString stationIdPath = QString("%1/%1_stationId").arg(name);

    if (!m_settings->contains(ipPath)){
        m_mainWindow.setLogLine(QString("IP адрес %1 не определен").arg(m_stationNames[name]), MainWindow::LogTarget::ALL);
        return;
    }

    if (!m_settings->contains(portPath)){
        m_mainWindow.setLogLine(QString("Порт %1 не определен").arg(m_stationNames[name]), MainWindow::LogTarget::ALL);
        return;
    }

    if (!m_settings->contains(periodPath)){
        m_mainWindow.setLogLine(QString("Период опроса %1 не определен").arg(m_stationNames[name]), MainWindow::LogTarget::ALL);
        return;
    }

    if (!m_settings->contains(stationIdPath)){
        m_mainWindow.setLogLine(QString("Номер станции %1 не определен").arg(m_stationNames[name]), MainWindow::LogTarget::ALL);
        return;
    }

    QString ip = m_settings->value(ipPath).toString();
    int port = m_settings->value(portPath).toInt();
    int period = m_settings->value(periodPath).toInt()*1000;
    int stationId = m_settings->value(stationIdPath).toInt();

    PumpStationDefinition def;
    def = {name, stationId, 0, 4};
    m_knsModbusDefinitions->push_back(def);

    CSModbusCollector* modbusCollector = new CSModbusCollector(this);
    modbusCollector->setIpAddress(ip);
    modbusCollector->setPort(port);
    modbusCollector->setRequestPeriod(period);
    modbusCollector->setFirstRegister(def.firstRegister);
    modbusCollector->setRegistersCount(def.registerCount);
    modbusCollector->setStationId(def.stationId);
    modbusCollector->setLogFormat(def.name + " -> %1");
    connect(modbusCollector, SIGNAL(newDumpLog(QString)), &m_mainWindow, SLOT(setLogLine(QString)));
    connect(modbusCollector, SIGNAL(newLog(QString)), &m_mainWindow, SLOT(setLogLine(QString)));
    connect(modbusCollector, SIGNAL(newRegistersArray(CSRegistersArray)), SLOT(newModbusKnsRegisterArray(CSRegistersArray)));

    m_knsModbusCollectors[def.name] = modbusCollector;
    modbusCollector->startRequesting();
}


void Server::requestVNS(const QString& name)
{
    if (m_vnsFatekCollectors.contains(name)) {
        CSFatekCollector* collector = m_vnsFatekCollectors.value(name);
        collector->reconnect();
    }
}


void Server::setKosk()
{
    //КОСК
    if (!m_settings->contains("kosks/kosks")){
        m_mainWindow.setLogLine("Список ПЛК КОСК (kosks) неопределен", MainWindow::LogTarget::ALL);
        return;
    }

    QString s_kosks = m_settings->value("kosks/kosks").toString();
    m_koskIds = s_kosks.simplified().split(" ");

    foreach(QString koskId, m_koskIds){
        if (!m_settings->contains(QString("kosks/kosk%1_ip").arg(koskId))){
            m_mainWindow.setLogLine(QString("IP адрес ПЛК КОСК №%1 (kosk%1_ip) не определен").arg(koskId), MainWindow::LogTarget::ALL);
            return;
        }

        if (!m_settings->contains(QString("kosks/kosk%1_port").arg(koskId))){
            m_mainWindow.setLogLine(QString("Порт ПЛК КОСК №%1 (kosk%1_port) не определен").arg(koskId), MainWindow::LogTarget::ALL);
            return;
        }

        if (!m_settings->contains(QString("kosks/kosk%1_period").arg(koskId))){
            m_mainWindow.setLogLine(QString("Период опроса ПЛК КОСК №%1 (kosk%1_period) не определен").arg(koskId), MainWindow::LogTarget::ALL);
            return;
        }

        QString kosk_ip = m_settings->value(QString("kosks/kosk%1_ip").arg(koskId)).toString();
        int kosk_port = m_settings->value(QString("kosks/kosk%1_port").arg(koskId)).toInt();
        int kosk_period = m_settings->value(QString("kosks/kosk%1_period").arg(koskId)).toInt()*1000;
        m_koskFatekCollectors.insert(koskId, new CSFatekCollector(this));
        m_koskFatekCollectors[koskId]->setIpAddress(kosk_ip);
        m_koskFatekCollectors[koskId]->setPort(kosk_port);
        m_koskFatekCollectors[koskId]->setRequestPeriod(kosk_period);
        m_koskFatekCollectors[koskId]->setFirstRegister(0);
        m_koskFatekCollectors[koskId]->setRegistersNumber(16);
        m_koskFatekCollectors[koskId]->addStationId(koskId.toInt());
        m_koskFatekCollectors[koskId]->setLogFormat(QString("kosk%1 -> %2").arg(koskId, "%1"));
        m_koskFatekCollectors[koskId]->setStationString("kosk");
        connect(m_koskFatekCollectors[koskId], SIGNAL(newDumpLog(QString)), &m_mainWindow, SLOT(setLogLine(QString)));
        connect(m_koskFatekCollectors[koskId], SIGNAL(newLog(QString)), &m_mainWindow, SLOT(setLogLine(QString)));
        connect(m_koskFatekCollectors[koskId], SIGNAL(newRegistersArray(CSRegistersArray)), SLOT(newKosksRegisterArray(CSRegistersArray)));
        m_koskFatekCollectors[koskId]->startRequesting();

        //connect(m_koskFatekCollectors[koskId], SIGNAL(sended(quint32,bool)), SLOT(toKoskSended(quint32,bool)));
    }
}

void Server::newKosksRegisterArray(const CSRegistersArray &registerArray)
{
    QString koskName = "kosk";

    {
        // Время последнего соединения
        QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
        m_sqlstorage->archiveRecord(koskName, "lastConnection", datetime);

        /*if (!m_sqlstorage->isSame(vns1Name, "linkState", "0")){
            m_sqlstorage->archiveRecord(vns1Name, "linkState", "0");
            m_sqlstorage->archiveRecord(vns1Name, "mess", m_stationNames[vns1Name]+": связь восстановлена;#60FF60", ToStorage);
        }*/
    }

    if (registerArray.stationAddress == 1){
        quint16 R0 = registerArray.getRegister(0);
        m_sqlstorage->archiveRecord(koskName, "pump1GridState", (R0 & 0x2)?1:0, OnChange|ToStorage);
        m_sqlstorage->archiveRecord(koskName, "pump2GridState", (R0 & 0x4)?1:0, OnChange|ToStorage);
        m_sqlstorage->archiveRecord(koskName, "pump3GridState", (R0 & 0x8)?1:0, OnChange|ToStorage);

        m_sqlstorage->archiveRecord(koskName, "pumpOsadok1State", (R0 & 0x10)?1:0, OnChange|ToStorage);
        m_sqlstorage->archiveRecord(koskName, "pumpOsadok2State", (R0 & 0x20)?1:0, OnChange|ToStorage);
        m_sqlstorage->archiveRecord(koskName, "pumpOsadok3State", (R0 & 0x40)?1:0, OnChange|ToStorage);
        m_sqlstorage->archiveRecord(koskName, "pumpOsadok4State", (R0 & 0x80)?1:0, OnChange|ToStorage);

        m_sqlstorage->archiveRecord(koskName, "primary1State", (R0 & 0x100)?1:0, OnChange|ToStorage);
        m_sqlstorage->archiveRecord(koskName, "primary2State", (R0 & 0x200)?1:0, OnChange|ToStorage);
        m_sqlstorage->archiveRecord(koskName, "primary3State", (R0 & 0x400)?1:0, OnChange|ToStorage);
        m_sqlstorage->archiveRecord(koskName, "primary4State", (R0 & 0x800)?1:0, OnChange|ToStorage);

        int n1 = (R0 & 0x100)?1:0;
        int n2 = (R0 & 0x200)?1:0;
        int n3 = (R0 & 0x400)?1:0;
        int n4 = (R0 & 0x800)?1:0;

        if (n1 | n2 | n3 | n4){
            m_sqlstorage->archiveRecord(koskName, "noneSignal", "1", OnChange);
            if (!m_sqlstorage->isSame(koskName, "noneState", "0")){
                m_sqlstorage->archiveRecord(koskName, "noneState", "0");
                m_sqlstorage->archiveRecord(koskName, "mess", m_stationNames[koskName]+": бездействие отстойников прекратилось;#60FF60", ToStorage);
            }
        } else {
            m_sqlstorage->archiveRecord(koskName, "noneSignal", "0", OnChange);
        }

        double value;

        value = registerArray.getRegister(1);
        value = value / 10;
        m_sqlstorage->archiveRecord(koskName, "pump1GridValue", value, ToStorage);

        value = registerArray.getRegister(2);
        value = value / 10;
        m_sqlstorage->archiveRecord(koskName, "pump2GridValue", value, ToStorage);

        value = registerArray.getRegister(3);
        value = value / 10;
        m_sqlstorage->archiveRecord(koskName, "pump3GridValue", value, ToStorage);


        value = registerArray.getRegister(4);
        value = value / 10;
        m_sqlstorage->archiveRecord(koskName, "pumpOsadok1Value", value, ToStorage);

        value = registerArray.getRegister(5);
        value = value / 10;
        m_sqlstorage->archiveRecord(koskName, "pumpOsadok2Value", value, ToStorage);

        value = registerArray.getRegister(6);
        value = value / 10;
        m_sqlstorage->archiveRecord(koskName, "pumpOsadok3Value", value, ToStorage);

        value = registerArray.getRegister(7);
        value = value / 10;
        m_sqlstorage->archiveRecord(koskName, "pumpOsadok4Value", value, ToStorage);


        value = registerArray.getRegister(8);
        value = value / 10;
        m_sqlstorage->archiveRecord(koskName, "primary1Value", value, ToStorage);

        value = registerArray.getRegister(9);
        value = value / 10;
        m_sqlstorage->archiveRecord(koskName, "primary2Value", value, ToStorage);

        value = registerArray.getRegister(10);
        value = value / 10;
        m_sqlstorage->archiveRecord(koskName, "primary3Value", value, ToStorage);

        value = registerArray.getRegister(11);
        value = value / 10;
        m_sqlstorage->archiveRecord(koskName, "primary4Value", value, ToStorage);

        int pos;
        pos = registerArray.getRegister(12);
        m_sqlstorage->archiveRecord(koskName, "primaryPosition1Value", pos, OnChange);
        pos = registerArray.getRegister(13);
        m_sqlstorage->archiveRecord(koskName, "primaryPosition2Value", pos, OnChange);
        pos = registerArray.getRegister(14);
        m_sqlstorage->archiveRecord(koskName, "primaryPosition3Value", pos, OnChange);
        pos = registerArray.getRegister(15);
        m_sqlstorage->archiveRecord(koskName, "primaryPosition4Value", pos, OnChange);

        return;
    }

    if (registerArray.stationAddress == 2){
        quint16 R0 = registerArray.getRegister(0);
        m_sqlstorage->archiveRecord(koskName, "pump1DrenState", (R0 & 0x2)?1:0, OnChange|ToStorage);
        m_sqlstorage->archiveRecord(koskName, "pump2DrenState", (R0 & 0x4)?1:0, OnChange|ToStorage);
        m_sqlstorage->archiveRecord(koskName, "pump3DrenState", (R0 & 0x8)?1:0, OnChange|ToStorage);

        m_sqlstorage->archiveRecord(koskName, "secondary1State", (R0 & 0x10)?1:0, OnChange|ToStorage);
        m_sqlstorage->archiveRecord(koskName, "secondary2State", (R0 & 0x20)?1:0, OnChange|ToStorage);
        m_sqlstorage->archiveRecord(koskName, "secondary3State", (R0 & 0x40)?1:0, OnChange|ToStorage);
        m_sqlstorage->archiveRecord(koskName, "secondary4State", (R0 & 0x80)?1:0, OnChange|ToStorage);

        double value;

        value = registerArray.getRegister(1);
        value = value / 10;
        m_sqlstorage->archiveRecord(koskName, "pump1DrenValue", value, ToStorage);

        value = registerArray.getRegister(2);
        value = value / 10;
        m_sqlstorage->archiveRecord(koskName, "pump2DrenValue", value, ToStorage);

        value = registerArray.getRegister(3);
        value = value / 10;
        m_sqlstorage->archiveRecord(koskName, "pump3DrenValue", value, ToStorage);


        value = registerArray.getRegister(4);
        value = value / 10;
        m_sqlstorage->archiveRecord(koskName, "secondary1Value", value, ToStorage);

        value = registerArray.getRegister(5);
        value = value / 10;
        m_sqlstorage->archiveRecord(koskName, "secondary2Value", value, ToStorage);

        value = registerArray.getRegister(6);
        value = value / 10;
        m_sqlstorage->archiveRecord(koskName, "secondary3Value", value, ToStorage);

        value = registerArray.getRegister(7);
        value = value / 10;
        m_sqlstorage->archiveRecord(koskName, "secondary4Value", value, ToStorage);


        int pos;
        pos = registerArray.getRegister(8);
        m_sqlstorage->archiveRecord(koskName, "secondaryPosition1Value", pos, OnChange);
        pos = registerArray.getRegister(9);
        m_sqlstorage->archiveRecord(koskName, "secondaryPosition2Value", pos, OnChange);
        pos = registerArray.getRegister(10);
        m_sqlstorage->archiveRecord(koskName, "secondaryPosition3Value", pos, OnChange);
        pos = registerArray.getRegister(11);
        m_sqlstorage->archiveRecord(koskName, "secondaryPosition4Value", pos, OnChange);

        quint16 R12 = registerArray.getRegister(12);
        quint16 R13 = registerArray.getRegister(13);
        quint32 inning = R13 * 0x10000 + R12;
        m_sqlstorage->archiveRecord(koskName, "inningValue", (int)inning, ToStorage);

        quint16 R14 = registerArray.getRegister(14);
        quint16 R15 = registerArray.getRegister(15);
        quint32 consumption = R15 * 0x10000 + R14;
        m_sqlstorage->archiveRecord(koskName, "consumptionValue", (int)consumption, ToStorage);

        return;
    }
}

void Server::setBores()
{
    //Скважины
    if (!m_settings->contains("bores/bores")){
        m_mainWindow.setLogLine("Список скаважин (bores) неопределен", MainWindow::LogTarget::ALL);
        return;
    }

    QString s_bores = m_settings->value("bores/bores").toString();
    m_boreIds = s_bores.simplified().split(" ");

    foreach(QString boreId, m_boreIds){
        if (!m_settings->contains(QString("bores/bore%1_ip").arg(boreId))){
            m_mainWindow.setLogLine(QString("IP адрес ПЛК скважины №%1 (bore%1_ip) не определен").arg(boreId), MainWindow::LogTarget::ALL);
            return;
        }

        if (!m_settings->contains(QString("bores/bore%1_port").arg(boreId))){
            m_mainWindow.setLogLine(QString("Порт ПЛК скважины №%1 (bore%1_port) не определен").arg(boreId), MainWindow::LogTarget::ALL);
            return;
        }

        if (!m_settings->contains(QString("bores/bore%1_period").arg(boreId))){
            m_mainWindow.setLogLine(QString("Период опроса скважины №%1 (bore%1_period) не определен").arg(boreId), MainWindow::LogTarget::ALL);
            return;
        }

        m_mainWindow.bore->ui->boreComboBox->addItem(boreId);

        QString bore_ip = m_settings->value(QString("bores/bore%1_ip").arg(boreId)).toString();
        int bore_port = m_settings->value(QString("bores/bore%1_port").arg(boreId)).toInt();
        int bore_period = m_settings->value(QString("bores/bore%1_period").arg(boreId)).toInt()*1000;
        m_boreFatekCollectors.insert(boreId, new CSFatekCollector(this));
        m_boreFatekCollectors[boreId]->setIpAddress(bore_ip);
        m_boreFatekCollectors[boreId]->setPort(bore_port);
        m_boreFatekCollectors[boreId]->setRequestPeriod(bore_period);
        m_boreFatekCollectors[boreId]->setFirstRegister(0);
        m_boreFatekCollectors[boreId]->setRegistersNumber(8);
        m_boreFatekCollectors[boreId]->addStationId(boreId.toInt());
        m_boreFatekCollectors[boreId]->setLogFormat(QString("bore%1 -> %2").arg(boreId, "%1"));
        m_boreFatekCollectors[boreId]->setStationString("bore");
        connect(m_boreFatekCollectors[boreId], SIGNAL(newDumpLog(QString)), &m_mainWindow, SLOT(setLogLine(QString)));
        connect(m_boreFatekCollectors[boreId], SIGNAL(newLog(QString)), &m_mainWindow, SLOT(setLogLine(QString)));
        connect(m_boreFatekCollectors[boreId], SIGNAL(newRegistersArray(CSRegistersArray)), SLOT(newBoresRegisterArray(CSRegistersArray)));
        m_boreFatekCollectors[boreId]->startRequesting();
        connect(m_boreFatekCollectors[boreId], SIGNAL(sended(quint32,bool)), SLOT(toBoreSended(quint32,bool)));
    }

    connect(m_mainWindow.bore->ui->boreOnButton, SIGNAL(clicked(bool)), SLOT(boreSwitchOn()));
    connect(m_mainWindow.bore->ui->boreOffButton, SIGNAL(clicked(bool)), SLOT(boreSwitchOff()));
}

void Server::newBoresRegisterArray(const CSRegistersArray &registerArray)
{
    // Считаем что все скважины одинаковые
    QString boreName = "bore" + QString::number(registerArray.stationAddress);

    {
        // Время последнего соединения
        QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
        m_sqlstorage->archiveRecord(boreName, "lastConnection", datetime);

        if (!m_sqlstorage->isSame(boreName, "linkState", "0")){
            m_sqlstorage->archiveRecord(boreName, "linkState", "0");
            m_sqlstorage->archiveRecord(boreName, "mess", m_stationNames[boreName]+": связь восстановлена;#60FF60", ToStorage);
        }
    }

    quint16 R0 = registerArray.getRegister(0);
    m_sqlstorage->archiveTristate(boreName, "powerState", !(R0 & 0x1), "Питание");
    m_sqlstorage->archiveTristate(boreName, "openState", (R0 & 0x2), "Проникновение");
    m_sqlstorage->archiveTristate(boreName, "fireState", !(R0 & 0x4), "Пожар");
    m_sqlstorage->archiveRecord(boreName, "warmState", (R0 & 0x8)?0:1, OnChange);
    m_sqlstorage->archiveTristate(boreName, "tempState", !(R0 & 0x10), "Температура");
    if (R0 & 0x40){
        if (!m_sqlstorage->isSame(boreName, "pump1State", "2") && !m_sqlstorage->isSame(boreName, "pump1State", "1")){
            m_sqlstorage->archiveRecord(boreName, "mess", boreName+": Авария насоса;#FF6060", ToStorage);
            m_sqlstorage->archiveRecord(boreName, "pump1State", 2, OnChange | ToStorage);
        }
    } else {
        if (m_sqlstorage->isSame(boreName, "pump1State", "2") || m_sqlstorage->isSame(boreName, "pump1State", "1")){
            m_sqlstorage->archiveRecord(boreName, "mess", boreName+": Состояние насоса вернулось в норму;#60FF60", ToStorage);
        }
        m_sqlstorage->archiveRecord(boreName, "pump1State", (R0 & 0x20)?3:0, OnChange | ToStorage);
    }
    double R1 = registerArray.getRegister(1);
    m_sqlstorage->archiveRecord(boreName, "curValue", R1, OnChange | ToStorage);
    m_sqlstorage->archiveRecord(boreName, "curPerValue", R1*100/98, OnChange | ToStorage);
    quint16 R2 = registerArray.getRegister(2);
    quint16 R3 = registerArray.getRegister(3);
    quint32 motoHour = R3 * 0x10000 + R2;
    m_sqlstorage->archiveRecord(boreName, "pump1Value", (int)motoHour, OnChange | ToStorage);
    quint16 R4 = registerArray.getRegister(4);
    quint16 R5 = registerArray.getRegister(5);
    int consumption = R5 * 0x10000 + R4;
    consumption = (consumption < 0) ? 0 : consumption;
    m_sqlstorage->archiveRecord(boreName, "mh1Value", (int)consumption, OnChange | ToStorage);
    m_sqlstorage->archiveRecord(boreName, "mhsValue", (int)consumption, OnChange | ToStorage);
    quint16 R6 = registerArray.getRegister(6);
    quint16 R7 = registerArray.getRegister(7);
    quint32 extraction = R7 * 0x10000 + R6;
    m_sqlstorage->archiveRecord(boreName, "m1Value", (int)extraction, OnChange | ToStorage);
    m_sqlstorage->archiveRecord(boreName, "msValue", (int)extraction, OnChange | ToStorage);
}

void Server::setModbusServer()
{
    //modbus
    if (!m_settings->contains("server/modbus_ip")){
        m_mainWindow.setLogLine("IP адрес сервера modbus (modbus_ip) не определен", MainWindow::LogTarget::ALL);
        return;
    }

    if (!m_settings->contains("server/modbus_port")){
        m_mainWindow.setLogLine("Порт сервера modbus (modbus_port) не определен", MainWindow::LogTarget::ALL);
        return;
    }

    QString modbus_ip = m_settings->value("server/modbus_ip").toString();
    int modbus_port = m_settings->value("server/modbus_port").toInt();
    m_modbusServer = new CSModbusServer(modbus_ip, modbus_port, this);

    if (m_modbusServer->isListining()){
        m_mainWindow.setLogLine("Сервер ожидает modbus пакеты");
    } else {
        m_mainWindow.setLogLine("Сервер не подключает modbus соединения: " + m_modbusServer->getServer()->errorString(), MainWindow::LogTarget::ALL);
    }

    connect(m_modbusServer, SIGNAL(newLog(QString)), &m_mainWindow, SLOT(setLogLine(QString)));
    connect(m_modbusServer, SIGNAL(newDumpLog(QString)), &m_mainWindow, SLOT(setLogLine(QString)));
    connect(m_modbusServer, SIGNAL(newRegistersArray(CSRegistersArray)), SLOT(newModbusRegisterArray(CSRegistersArray)));
}

void Server::dumpModbusMsg(const QString &msg) {
    m_sqlstorage->archiveRecord("server", "modbusMsg", msg, ToStorage);
}

void Server::newModbusRegisterArray(const CSRegistersArray &registerArray)
{
    QString station;
    station = "_bs";
    /*if (registerArray.stationAddress == 3){
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(station, "linkState", "0")){
                m_sqlstorage->archiveRecord(station, "linkState", "0");
                m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            //Расход Q1 м3/ч
            int R = 0;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "mh1Value", value, OnChange | ToStorage);
            }
        }
        {
            //Объем V1, м3
            int RH = 7;
            int RL = 8;
            if (registerArray.isWithRegister(RH) && registerArray.isWithRegister(RL)){
                int hvalue = registerArray.getRegister(RH);
                int lvalue = registerArray.getRegister(RL);
                int value = lvalue + hvalue*0x10000;
                m_sqlstorage->archiveRecord(station, "m1Value", value, OnChange | ToStorage);
            }
        }
        {
            //Давление входное
            int R = 5;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "presValue", value/10, ToStorage);
            }
        }
        {
            //Дискретные датчики
            int R = 1;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                //Состояния
                m_sqlstorage->archiveTristate(station, "openState", (value & 0x2), "Вскрытие");
                m_sqlstorage->archiveTristate(station, "tempState", (value & 0x4), "Температура");
                m_sqlstorage->archiveTristate(station, "powerFailureState", (value & 0x8), "Неполадка питания");
                m_sqlstorage->archiveTristate(station, "powerLowState", (value & 0x10), "Пропадание питания");
            }
        }
        return;
    }*/
    /*if (registerArray.stationAddress == 4) {
        station="vns8";
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(station, "linkState", "0")){
                m_sqlstorage->archiveRecord(station, "linkState", "0");
                m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            // Частота
            int R = 4;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value /= 10.0;
                m_sqlstorage->archiveRecord(station, "freq1Value", value, ToStorage);
            }
        }
        {
            //Ток
            int R = 5;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value /= 100.0;
                m_sqlstorage->archiveRecord(station, "cur1Value", value, ToStorage);
            }
            //% Тока от номинала
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 5.0;
                m_sqlstorage->archiveRecord(station, "curPer1Value", value, ToStorage);
            }
        }
        {
            // Частота
            int R = 7;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value /= 10.0;
                m_sqlstorage->archiveRecord(station, "freq2Value", value, ToStorage);
            }
        }
        {
            //Ток
            int R = 8;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value /= 100.0;
                m_sqlstorage->archiveRecord(station, "cur2Value", value, ToStorage);
            }
            //% Тока от номинала
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 5.0;
                m_sqlstorage->archiveRecord(station, "curPer2Value", value, ToStorage);
            }
        }
        {
            //Давление входное
            int R = 10;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value /= 100.0;
                m_sqlstorage->archiveRecord(station, "presInValue", value, ToStorage);
            }
        }
        {
            //Давление выходное
            int R = 11;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                value = value / 100.0;
                if (value >= 0 && value <= 8.0) {
                    m_sqlstorage->archiveRecord(station, "presOutValue", value, ToStorage);
                }
            }
        }
        {
            //Моточасы 1 насос
            int R = 6;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 9;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
            }
        }
        {
            //Дискретные датчики
            int R = 3;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                //Состояния
                m_sqlstorage->archiveTristate(station, "overState", (value & 0x1), "Затопление");
                m_sqlstorage->archiveTristate(station, "powerState", (value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(station, "openState", (value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(station, "tempState", (value & 0x8), "Температура");
                m_sqlstorage->archiveTristate(station, "fireState", (value & 0x10), "Задымление");

                //Состояние насосов
                m_sqlstorage->archiveRecord(station, "pump1State", (value & 0x400), OnChange | ToStorage);
                m_sqlstorage->archiveRecord(station, "pump2State", (value & 0x800), OnChange | ToStorage);

                // ПЧ
                m_sqlstorage->archiveTristate(station, "errorPC1", (value & 0x1000), "Ошибка ПЧ1");
                m_sqlstorage->archiveTristate(station, "errorPC2", (value & 0x2000), "Ошибка ПЧ2");
                m_sqlstorage->archiveTristate(station, "linkPC1", (value & 0x4000), "Связь с ПЧ1");
                m_sqlstorage->archiveTristate(station, "linkPC2", (value & 0x8000), "Связь с ПЧ2");
            }
        }
        return;
    }*/
    // КНС-1
    /*station = "kns1";
    if (registerArray.stationAddress == 5){
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(station, "linkState", "0")){
                m_sqlstorage->archiveRecord(station, "linkState", "0");
                m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            // Частота
            int R = 3;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "freqValue", value, ToStorage);
            }
        }
        {
            //Ток
            int R = 4;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getAsciiRegister(R);
                m_sqlstorage->archiveRecord(station, "curValue", value, ToStorage);
            }
            //% Тока от номинала
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                value = value * 100 / 98;
                m_sqlstorage->archiveRecord(station, "curPerValue", value, ToStorage);
            }
        }
        {
            //Давление входное
            int R = 5;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "presInValue", value, ToStorage);
            }
        }
        {
            //Давление выходное
            int R = 6;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                m_sqlstorage->archiveRecord(station, "presOutValue", value, ToStorage);
            }
        }
        {
            //Уровень сигнала
            int R = 0;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                value = value * 200 / 112;
                m_sqlstorage->archiveRecord(station, "levelValue", value, ToStorage);
            }
        }
        {
            //Моточасы 1 насос
            int R = 7;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 8;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
            }
        }
        {
            //Моточасы 3 насос
            int R = 9;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump3Value", value, ToStorage);
            }
        }
        {
            //Моточасы 4 насос
            int R = 10;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump4Value", value, ToStorage);
            }
        }
        {
            //Моточасы 5 насос
            int R = 11;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump5Value", value, ToStorage);
            }
        }
        {
            //Состояние насосов
            int R = 2;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1State", (value & 0x2)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(station, "pump2State", (value & 0x4)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(station, "pump3State", (value & 0x8)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(station, "pump4State", (value & 0x10)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(station, "pump5State", (value & 0x20)?1:0, OnChange | ToStorage);

                int n1 = (value & 0x2)?1:0;
                int n2 = (value & 0x4)?1:0;
                int n3 = (value & 0x8)?1:0;
                int n4 = (value & 0x10)?1:0;
                int n5 = (value & 0x20)?1:0;

                if (n1 | n2 | n3 | n4 | n5){
                    m_sqlstorage->archiveRecord(station, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(station, "noneState", "0")){
                        m_sqlstorage->archiveRecord(station, "noneState", "0");
                        m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(station, "noneSignal", "0", OnChange);
                }
            }
        }
        {
            //Состояния
            int R = 1;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveTristate(station, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(station, "openState", !(value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(station, "fireState", !(value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(station, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(station, "overState", (value & 0x20), "Затопление");
                m_sqlstorage->archiveTristate(station, "highState", (value & 0x40), "Высокий уровень");
                m_sqlstorage->archiveTristate(station, "lowState", (value & 0x80), "Низкий уровень");
            }
        }
        return;
    }*/
    // КНС-3
    /*station = "kns3";
    if (registerArray.stationAddress == 6){
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(station, "linkState", "0")){
                m_sqlstorage->archiveRecord(station, "linkState", "0");
                m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            //Уровень сигнала
            int R = 0;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                value = value * 200 / 112;
                m_sqlstorage->archiveRecord(station, "levelValue", value, ToStorage);
            }
        }
        {
            //Моточасы 1 насос
            int R = 7;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 8;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
            }
        }
        {
            //Моточасы 3 насос
            int R = 9;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump3Value", value, ToStorage);
            }
        }
        {
            //Состояние насосов
            int R = 2;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1State", (value & 0x2)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(station, "pump2State", (value & 0x4)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(station, "pump3State", (value & 0x8)?1:0, OnChange | ToStorage);

                int n1 = (value & 0x2)?1:0;
                int n2 = (value & 0x4)?1:0;
                int n3 = (value & 0x8)?1:0;

                if (n1 | n2 | n3){
                    m_sqlstorage->archiveRecord(station, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(station, "noneState", "0")){
                        m_sqlstorage->archiveRecord(station, "noneState", "0");
                        m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(station, "noneSignal", "0", OnChange);
                }
            }
        }
        {
            //Состояния
            int R = 1;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveTristate(station, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(station, "openState", !(value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(station, "fireState", !(value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(station, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(station, "overState", (value & 0x20), "Затопление");
                m_sqlstorage->archiveTristate(station, "highState", (value & 0x40), "Высокий уровень");
                m_sqlstorage->archiveTristate(station, "lowState", (value & 0x80), "Низкий уровень");
            }
        }
        return;
    }
    // КНС-4
    station = "kns4";
    if (registerArray.stationAddress == 7){
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(station, "linkState", "0")){
                m_sqlstorage->archiveRecord(station, "linkState", "0");
                m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            //Уровень сигнала
            int R = 0;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                value = value * 200 / 112;
                m_sqlstorage->archiveRecord(station, "levelValue", value, ToStorage);
            }
        }
        {
            //Моточасы 1 насос
            int R = 7;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 8;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
            }
        }
        {
            //Состояние насосов
            int R = 2;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1State", (value & 0x2)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(station, "pump2State", (value & 0x4)?1:0, OnChange | ToStorage);

                int n1 = (value & 0x2)?1:0;
                int n2 = (value & 0x4)?1:0;

                if (n1 | n2 ){
                    m_sqlstorage->archiveRecord(station, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(station, "noneState", "0")){
                        m_sqlstorage->archiveRecord(station, "noneState", "0");
                        m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(station, "noneSignal", "0", OnChange);
                }
            }
        }
        {
            //Состояния
            int R = 1;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveTristate(station, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(station, "openState", !(value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(station, "fireState", !(value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(station, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(station, "overState", (value & 0x20), "Затопление");
                m_sqlstorage->archiveTristate(station, "highState", (value & 0x40), "Высокий уровень");
                m_sqlstorage->archiveTristate(station, "lowState", (value & 0x80), "Низкий уровень");
            }
        }
        return;
    }
    // КНС-5
    station = "kns5";
    if (registerArray.stationAddress == 8){
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(station, "linkState", "0")){
                m_sqlstorage->archiveRecord(station, "linkState", "0");
                m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            //Уровень сигнала
            int R = 0;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                value = value * 200 / 112;
                m_sqlstorage->archiveRecord(station, "levelValue", value, ToStorage);
            }
        }
        {
            //Моточасы 1 насос
            int R = 7;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 8;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
            }
        }
        {
            //Состояние насосов
            int R = 2;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1State", (value & 0x2)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(station, "pump2State", (value & 0x4)?1:0, OnChange | ToStorage);

                int n1 = (value & 0x2)?1:0;
                int n2 = (value & 0x4)?1:0;

                if (n1 | n2 ){
                    m_sqlstorage->archiveRecord(station, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(station, "noneState", "0")){
                        m_sqlstorage->archiveRecord(station, "noneState", "0");
                        m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(station, "noneSignal", "0", OnChange);
                }
            }
        }
        {
            //Состояния
            int R = 1;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveTristate(station, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(station, "openState", !(value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(station, "fireState", !(value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(station, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(station, "overState", (value & 0x20), "Затопление");
                m_sqlstorage->archiveTristate(station, "highState", (value & 0x40), "Высокий уровень");
                m_sqlstorage->archiveTristate(station, "lowState", (value & 0x80), "Низкий уровень");
            }
        }
        return;
    }
    // КНС-6
    station = "kns6";
    if (registerArray.stationAddress == 9){
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(station, "linkState", "0")){
                m_sqlstorage->archiveRecord(station, "linkState", "0");
                m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            //Уровень сигнала
            int R = 0;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                value = value * 200 / 112;
                m_sqlstorage->archiveRecord(station, "levelValue", value, ToStorage);
            }
        }
        {
            //Моточасы 1 насос
            int R = 7;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 8;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
            }
        }
        {
            //Состояние насосов
            int R = 2;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1State", (value & 0x2)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(station, "pump2State", (value & 0x4)?1:0, OnChange | ToStorage);

                int n1 = (value & 0x2)?1:0;
                int n2 = (value & 0x4)?1:0;

                if (n1 | n2 ){
                    m_sqlstorage->archiveRecord(station, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(station, "noneState", "0")){
                        m_sqlstorage->archiveRecord(station, "noneState", "0");
                        m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(station, "noneSignal", "0", OnChange);
                }
            }
        }
        {
            //Состояния
            int R = 1;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveTristate(station, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(station, "openState", !(value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(station, "fireState", !(value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(station, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(station, "overState", (value & 0x20), "Затопление");
                m_sqlstorage->archiveTristate(station, "highState", (value & 0x40), "Высокий уровень");
                m_sqlstorage->archiveTristate(station, "lowState", (value & 0x80), "Низкий уровень");
            }
        }
        return;
    }
    // КНС-7
    /*station = "kns7";
    if (registerArray.stationAddress == 10){
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(station, "linkState", "0")){
                m_sqlstorage->archiveRecord(station, "linkState", "0");
                m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            //Уровень сигнала
            int R = 0;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                value = value * 200 / 112;
                m_sqlstorage->archiveRecord(station, "levelValue", value, ToStorage);
            }
        }
        {
            //Моточасы 1 насос
            int R = 7;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 8;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
            }
        }
        {
            //Состояние насосов
            int R = 2;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1State", (value & 0x2)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(station, "pump2State", (value & 0x4)?1:0, OnChange | ToStorage);

                int n1 = (value & 0x2)?1:0;
                int n2 = (value & 0x4)?1:0;

                if (n1 | n2 ){
                    m_sqlstorage->archiveRecord(station, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(station, "noneState", "0")){
                        m_sqlstorage->archiveRecord(station, "noneState", "0");
                        m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(station, "noneSignal", "0", OnChange);
                }
            }
        }
        {
            //Состояния
            int R = 1;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveTristate(station, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(station, "openState", !(value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(station, "fireState", !(value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(station, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(station, "overState", (value & 0x20), "Затопление");
                m_sqlstorage->archiveTristate(station, "highState", (value & 0x40), "Высокий уровень");
                m_sqlstorage->archiveTristate(station, "lowState", (value & 0x80), "Низкий уровень");
            }
        }
        return;
    }*/
    // КНС-8
    /*station = "kns8";
    if (registerArray.stationAddress == 11){
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(station, "linkState", "0")){
                m_sqlstorage->archiveRecord(station, "linkState", "0");
                m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            //Уровень сигнала
            int R = 0;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                value = value * 200 / 112;
                m_sqlstorage->archiveRecord(station, "levelValue", value, ToStorage);
            }
        }
        {
            //Моточасы 1 насос
            int R = 7;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 8;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
            }
        }
        {
            //Состояние насосов
            int R = 2;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1State", (value & 0x2)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(station, "pump2State", (value & 0x4)?1:0, OnChange | ToStorage);

                int n1 = (value & 0x2)?1:0;
                int n2 = (value & 0x4)?1:0;

                if (n1 | n2 ){
                    m_sqlstorage->archiveRecord(station, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(station, "noneState", "0")){
                        m_sqlstorage->archiveRecord(station, "noneState", "0");
                        m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(station, "noneSignal", "0", OnChange);
                }
            }
        }
        {
            //Состояния
            int R = 1;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveTristate(station, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(station, "openState", !(value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(station, "fireState", !(value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(station, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(station, "overState", (value & 0x20), "Затопление");
                m_sqlstorage->archiveTristate(station, "highState", (value & 0x40), "Высокий уровень");
                m_sqlstorage->archiveTristate(station, "lowState", (value & 0x80), "Низкий уровень");
            }
        }
        return;
    }
    // КНС-9
    station = "kns9";
    if (registerArray.stationAddress == 12){
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(station, "linkState", "0")){
                m_sqlstorage->archiveRecord(station, "linkState", "0");
                m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            //Уровень сигнала
            int R = 0;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                value = value * 200 / 112;
                m_sqlstorage->archiveRecord(station, "levelValue", value, ToStorage);
            }
        }
        {
            //Моточасы 1 насос
            int R = 7;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 8;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
            }
        }
        {
            //Состояние насосов
            int R = 2;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1State", (value & 0x2)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(station, "pump2State", (value & 0x4)?1:0, OnChange | ToStorage);

                int n1 = (value & 0x2)?1:0;
                int n2 = (value & 0x4)?1:0;

                if (n1 | n2 ){
                    m_sqlstorage->archiveRecord(station, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(station, "noneState", "0")){
                        m_sqlstorage->archiveRecord(station, "noneState", "0");
                        m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(station, "noneSignal", "0", OnChange);
                }
            }
        }
        {
            //Состояния
            int R = 1;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveTristate(station, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(station, "openState", !(value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(station, "fireState", !(value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(station, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(station, "overState", (value & 0x20), "Затопление");
                m_sqlstorage->archiveTristate(station, "highState", (value & 0x40), "Высокий уровень");
                m_sqlstorage->archiveTristate(station, "lowState", (value & 0x80), "Низкий уровень");
            }
        }
        return;
    }
    // КНС-10
    station = "kns10";
    if (registerArray.stationAddress == 13){
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(station, "linkState", "0")){
                m_sqlstorage->archiveRecord(station, "linkState", "0");
                m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            //Уровень сигнала
            int R = 0;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                value = value * 200 / 112;
                m_sqlstorage->archiveRecord(station, "levelValue", value, ToStorage);
            }
        }
        {
            //Моточасы 1 насос
            int R = 7;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 8;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
            }
        }
        {
            //Состояние насосов
            int R = 2;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1State", (value & 0x2)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(station, "pump2State", (value & 0x4)?1:0, OnChange | ToStorage);

                int n1 = (value & 0x2)?1:0;
                int n2 = (value & 0x4)?1:0;

                if (n1 | n2 ){
                    m_sqlstorage->archiveRecord(station, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(station, "noneState", "0")){
                        m_sqlstorage->archiveRecord(station, "noneState", "0");
                        m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(station, "noneSignal", "0", OnChange);
                }
            }
        }
        {
            //Состояния
            int R = 1;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveTristate(station, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(station, "openState", !(value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(station, "fireState", !(value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(station, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(station, "overState", (value & 0x20), "Затопление");
                m_sqlstorage->archiveTristate(station, "highState", (value & 0x40), "Высокий уровень");
                m_sqlstorage->archiveTristate(station, "lowState", (value & 0x80), "Низкий уровень");
            }
        }
        return;
    }
    // КНС-11
    station = "kns11";
    if (registerArray.stationAddress == 14){
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(station, "linkState", "0")){
                m_sqlstorage->archiveRecord(station, "linkState", "0");
                m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            //Уровень сигнала
            int R = 0;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                value = value * 200 / 112;
                m_sqlstorage->archiveRecord(station, "levelValue", value, ToStorage);
            }
        }
        {
            //Моточасы 1 насос
            int R = 7;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
            }
        }
        {
            //Состояние насосов
            int R = 2;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1State", (value & 0x2)?1:0, OnChange | ToStorage);

                int n1 = (value & 0x2)?1:0;

                if (n1){
                    m_sqlstorage->archiveRecord(station, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(station, "noneState", "0")){
                        m_sqlstorage->archiveRecord(station, "noneState", "0");
                        m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(station, "noneSignal", "0", OnChange);
                }
            }
        }
        {
            //Состояния
            int R = 1;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveTristate(station, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(station, "openState", !(value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(station, "fireState", !(value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(station, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(station, "overState", (value & 0x20), "Затопление");
                m_sqlstorage->archiveTristate(station, "highState", (value & 0x40), "Высокий уровень");
                m_sqlstorage->archiveTristate(station, "lowState", (value & 0x80), "Низкий уровень");
            }
        }
        return;
    }
    // КНС-12
    station = "kns12";
    if (registerArray.stationAddress == 15){
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(station, "linkState", "0")){
                m_sqlstorage->archiveRecord(station, "linkState", "0");
                m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            //Уровень сигнала
            int R = 0;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                value = value * 200 / 112;
                m_sqlstorage->archiveRecord(station, "levelValue", value, ToStorage);
            }
        }
        {
            //Моточасы 1 насос
            int R = 7;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 8;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
            }
        }
        {
            //Состояние насосов
            int R = 2;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1State", (value & 0x2)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(station, "pump2State", (value & 0x4)?1:0, OnChange | ToStorage);

                int n1 = (value & 0x2)?1:0;
                int n2 = (value & 0x4)?1:0;

                if (n1 | n2 ){
                    m_sqlstorage->archiveRecord(station, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(station, "noneState", "0")){
                        m_sqlstorage->archiveRecord(station, "noneState", "0");
                        m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(station, "noneSignal", "0", OnChange);
                }
            }
        }
        {
            //Состояния
            int R = 1;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveTristate(station, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(station, "openState", !(value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(station, "fireState", !(value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(station, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(station, "overState", (value & 0x20), "Затопление");
                m_sqlstorage->archiveTristate(station, "highState", (value & 0x40), "Высокий уровень");
                m_sqlstorage->archiveTristate(station, "lowState", (value & 0x80), "Низкий уровень");
            }
        }
        return;
    }
    // КНС-13
    station = "kns13";
    if (registerArray.stationAddress == 16){
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(station, "linkState", "0")){
                m_sqlstorage->archiveRecord(station, "linkState", "0");
                m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            //Уровень сигнала
            int R = 0;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                value = value * 200 / 112;
                m_sqlstorage->archiveRecord(station, "levelValue", value, ToStorage);
            }
        }
        {
            //Моточасы 1 насос
            int R = 7;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 8;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
            }
        }
        {
            //Состояние насосов
            int R = 2;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1State", (value & 0x2)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(station, "pump2State", (value & 0x4)?1:0, OnChange | ToStorage);

                int n1 = (value & 0x2)?1:0;
                int n2 = (value & 0x4)?1:0;

                if (n1 | n2 ){
                    m_sqlstorage->archiveRecord(station, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(station, "noneState", "0")){
                        m_sqlstorage->archiveRecord(station, "noneState", "0");
                        m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(station, "noneSignal", "0", OnChange);
                }
            }
        }
        {
            //Состояния
            int R = 1;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveTristate(station, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(station, "openState", !(value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(station, "fireState", !(value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(station, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(station, "overState", (value & 0x20), "Затопление");
                m_sqlstorage->archiveTristate(station, "highState", (value & 0x40), "Высокий уровень");
                m_sqlstorage->archiveTristate(station, "lowState", (value & 0x80), "Низкий уровень");
            }
        }
        return;
    }
    // КНС-14
    station = "kns14";
    if (registerArray.stationAddress == 17){
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(station, "linkState", "0")){
                m_sqlstorage->archiveRecord(station, "linkState", "0");
                m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            //Уровень сигнала
            int R = 0;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                value = value * 200 / 112;
                m_sqlstorage->archiveRecord(station, "levelValue", value, ToStorage);
            }
        }
        {
            //Моточасы 1 насос
            int R = 7;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 8;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
            }
        }
        {
            //Состояние насосов
            int R = 2;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1State", (value & 0x2)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(station, "pump2State", (value & 0x4)?1:0, OnChange | ToStorage);

                int n1 = (value & 0x2)?1:0;
                int n2 = (value & 0x4)?1:0;

                if (n1 | n2 ){
                    m_sqlstorage->archiveRecord(station, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(station, "noneState", "0")){
                        m_sqlstorage->archiveRecord(station, "noneState", "0");
                        m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(station, "noneSignal", "0", OnChange);
                }
            }
        }
        {
            //Состояния
            int R = 1;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveTristate(station, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(station, "openState", !(value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(station, "fireState", !(value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(station, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(station, "overState", (value & 0x20), "Затопление");
                m_sqlstorage->archiveTristate(station, "highState", (value & 0x40), "Высокий уровень");
                m_sqlstorage->archiveTristate(station, "lowState", (value & 0x80), "Низкий уровень");
            }
        }
        return;
    }
    // КНС-15
    station = "kns15";
    if (registerArray.stationAddress == 18){
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(station, "linkState", "0")){
                m_sqlstorage->archiveRecord(station, "linkState", "0");
                m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            //Уровень сигнала
            int R = 0;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                value = value * 200 / 112;
                m_sqlstorage->archiveRecord(station, "levelValue", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 8;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
            }
        }
        {
            //Состояние насосов
            int R = 2;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump2State", (value & 0x4)?1:0, OnChange | ToStorage);

                int n2 = (value & 0x4)?1:0;

                if (n2){
                    m_sqlstorage->archiveRecord(station, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(station, "noneState", "0")){
                        m_sqlstorage->archiveRecord(station, "noneState", "0");
                        m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(station, "noneSignal", "0", OnChange);
                }
            }
        }
        {
            //Состояния
            int R = 1;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveTristate(station, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(station, "openState", !(value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(station, "fireState", !(value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(station, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(station, "overState", (value & 0x20), "Затопление");
                m_sqlstorage->archiveTristate(station, "highState", (value & 0x40), "Высокий уровень");
                m_sqlstorage->archiveTristate(station, "lowState", (value & 0x80), "Низкий уровень");
            }
        }
        return;
    }
    // КНС-16
    station = "kns16";
    if (registerArray.stationAddress == 19){
        {
            // Время последнего соединения
            QString datetime = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
            m_sqlstorage->archiveRecord(station, "lastConnection", datetime);

            if (!m_sqlstorage->isSame(station, "linkState", "0")){
                m_sqlstorage->archiveRecord(station, "linkState", "0");
                m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": связь восстановлена;#60FF60", ToStorage);
            }
        }
        {
            //Уровень сигнала
            int R = 0;
            if (registerArray.isWithRegister(R)){
                double value = registerArray.getAsciiRegister(R);
                value = value * 200 / 112;
                m_sqlstorage->archiveRecord(station, "levelValue", value, ToStorage);
            }
        }
        {
            //Моточасы 1 насос
            int R = 7;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1Value", value, ToStorage);
            }
        }
        {
            //Моточасы 2 насос
            int R = 8;
            if (registerArray.isWithRegister(R)){
                int value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump2Value", value, ToStorage);
            }
        }
        {
            //Состояние насосов
            int R = 2;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveRecord(station, "pump1State", (value & 0x2)?1:0, OnChange | ToStorage);
                m_sqlstorage->archiveRecord(station, "pump2State", (value & 0x4)?1:0, OnChange | ToStorage);

                int n1 = (value & 0x2)?1:0;
                int n2 = (value & 0x4)?1:0;

                if (n1 | n2 ){
                    m_sqlstorage->archiveRecord(station, "noneSignal", "1", OnChange);
                    if (!m_sqlstorage->isSame(station, "noneState", "0")){
                        m_sqlstorage->archiveRecord(station, "noneState", "0");
                        m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов прекратилось;#60FF60", ToStorage);
                    }
                } else {
                    m_sqlstorage->archiveRecord(station, "noneSignal", "0", OnChange);
                }
            }
        }
        {
            //Состояния
            int R = 1;
            if (registerArray.isWithRegister(R)){
                quint16 value = registerArray.getRegister(R);
                m_sqlstorage->archiveTristate(station, "powerState", !(value & 0x2), "Питание");
                m_sqlstorage->archiveTristate(station, "openState", !(value & 0x4), "Вскрытие");
                m_sqlstorage->archiveTristate(station, "fireState", !(value & 0x8), "Задымление");
                m_sqlstorage->archiveTristate(station, "tempState", (value & 0x10), "Температура");
                m_sqlstorage->archiveTristate(station, "overState", (value & 0x20), "Затопление");
                m_sqlstorage->archiveTristate(station, "highState", (value & 0x40), "Высокий уровень");
                m_sqlstorage->archiveTristate(station, "lowState", (value & 0x80), "Низкий уровень");
            }
        }
        return;
    }*/
}

void Server::setSupervisorServer()
{
    //supervisor
    if (!m_settings->contains("server/supervisor_ip")){
        m_mainWindow.setLogLine("IP адрес сервера (supervisor_ip) не определен", MainWindow::LogTarget::ALL);
        return;
    }

    if (!m_settings->contains("server/supervisor_port")){
        m_mainWindow.setLogLine("Порт сервера (supervisor_port) не определен", MainWindow::LogTarget::ALL);
        return;
    }

    QString supervisor_ip = m_settings->value("server/supervisor_ip").toString();
    int supervisor_port = m_settings->value("server/supervisor_port").toInt();
    m_supervisorServer = new CSSupervisorServer(m_settings, supervisor_ip, supervisor_port, this);

    connect(m_supervisorServer, SIGNAL(newCommand(QString,QTcpSocket*)), SLOT(newCommand(QString,QTcpSocket*)));
    connect(m_supervisorServer, SIGNAL(newLog(QString)), &m_mainWindow, SLOT(setLogLine(QString)));
    connect(m_supervisorServer, SIGNAL(socketDisconnected(QTcpSocket*)), SLOT(socketDisconnected(QTcpSocket*)));

    if (m_supervisorServer->isListining()){
        m_mainWindow.setLogLine("Сервер ожидает диспетчерские пульты", MainWindow::LogTarget::ALL);
    } else {
        m_mainWindow.setLogLine("Сервер не подключает диспетчерские пульты: " + m_supervisorServer->getServer()->errorString(), MainWindow::LogTarget::ALL);
    }

    connect(m_supervisorServer, SIGNAL(userLogined(QString)), SLOT(userLogined(QString)));
    connect(m_supervisorServer, SIGNAL(userDisconnected(QString)), SLOT(userDisconnected(QString)));
}

void Server::userLogined(const QString &userName)
{
    m_sqlstorage->archiveRecord("server", "mess", "Пользователь "+userName+" вошел в систему", ToStorage);
}

void Server::userDisconnected(const QString &userName)
{
    m_sqlstorage->archiveRecord("server", "mess", "Пользователь "+userName+" вышел из системы", ToStorage);
}

void Server::setStorage()
{
    //sqlstorage
    if (!m_settings->contains("server/datapath")){
        m_mainWindow.setLogLine("Путь к данным (datapath) не определен", MainWindow::LogTarget::ALL);
        return;
    }

    m_sqlstorage = new CSSQLiteStorage(this);
    m_sqlstorage->setPath(m_settings->value("server/datapath").toString());

    m_sqlstorage->runQuery("server", "DROP TABLE IF EXISTS 'modbusMsg'");

    connect(m_sqlstorage, SIGNAL(newValue(QString,QString)), SLOT(newValue(QString,QString)));
}

void Server::toBoreSended(quint32, bool ok)
{
    if (ok){
        m_mainWindow.setLogLine("Команда успешно доставлена", MainWindow::LogTarget::ALL);
    } else {
        m_mainWindow.setLogLine("Ошибка доставки команды", MainWindow::LogTarget::ALL);
    }
}

void Server::boreSwitchOn()
{
    int boreId = m_mainWindow.bore->ui->boreComboBox->currentText().toInt();
    m_boreFatekCollectors[QString::number(boreId)]->sendRegister(boreId, 8, 1);
    m_mainWindow.bore->hide();
}

void Server::boreSwitchOff()
{
    int boreId = m_mainWindow.bore->ui->boreComboBox->currentText().toInt();
    m_boreFatekCollectors[QString::number(boreId)]->sendRegister(boreId, 8, 0);
    m_mainWindow.bore->hide();
}

void Server::socketDisconnected(QTcpSocket *socket)
{
    m_socketRegisters.remove(socket);
}

void Server::newValue(const QString &fullName, const QString &fullValue)
{
    for(QHash<QTcpSocket*, QSet<QString> >::iterator i = m_socketRegisters.begin(); i != m_socketRegisters.end(); i++){
        QTcpSocket *socket = i.key();
        QSet<QString> &set = i.value();
        if (set.contains(fullName)){
            sendReg(socket, fullName, fullValue);
        }
    }

    m_smsProcessor.processNewValue(fullName, fullValue, this);
}

void Server::sendReg(QTcpSocket *socket, const QString &fullName, const QString &fullValue)
{
    QString line = "reg|"+fullName+":"+fullValue;
    m_supervisorServer->sendToSocket(socket, line);
}

void Server::setPressure(const QString &name, const QString &strValue)
{
    quint8 stationId = 0;
    for(int i = 0; i < m_vnsFatekDefinitions->size(); ++i) {
        if (m_vnsFatekDefinitions->at(i).name == name) {
            stationId = (qint8)m_vnsFatekDefinitions->at(i).stationId;
            break;
        }
    }

    bool parseValue = false;
    QString presVal = strValue;
    float value = presVal.replace(',', '.').toFloat(&parseValue);
    qint16 regValue = (qint16)qFloor(16384 * value / 10.0);

    if (parseValue && value >= 0 && value <= 10.0 && stationId != 0 && m_vnsFatekCollectors.contains(name)) {
        CSFatekCollector* fatekCollector = m_vnsFatekCollectors[name];
        fatekCollector->sendRegister(stationId, 22, regValue);
    }
}

void Server::setKNSOverworkTime(const QString &stationName, const QString &strValue)
{
    quint8 stationId = 0;
    for (int i =0; i < m_knsFatekDefinitions->size(); ++i) {
        if (m_knsFatekDefinitions->at(i).name == stationName) {
            stationId = (qint8)m_knsFatekDefinitions->at(i).stationId;
        }
    }

    bool parseValue = false;
    QString timeValue = strValue;
    quint16 value = timeValue.toInt(&parseValue);

    if (parseValue && stationId != 0 && m_knsFatekCollectors.contains(stationName)) {
        CSFatekCollector* fatekCollector = m_knsFatekCollectors[stationName];
        fatekCollector->sendRegister(stationId, 8, value);
    }
}

void Server::setKNSInactivityTime(const QString &stationName, const QString &strValue)
{
    quint8 stationId = 0;
    for (int i =0; i < m_knsFatekDefinitions->size(); ++i) {
        if (m_knsFatekDefinitions->at(i).name == stationName) {
            stationId = (qint8)m_knsFatekDefinitions->at(i).stationId;
        }
    }

    bool parseValue = false;
    QString timeValue = strValue;
    quint16 value = timeValue.toInt(&parseValue);

    if (parseValue && stationId != 0 && m_knsFatekCollectors.contains(stationName)) {
        CSFatekCollector* fatekCollector = m_knsFatekCollectors[stationName];
        fatekCollector->sendRegister(stationId, 9, value);
    }
}

void Server::newCommand(const QString &line, QTcpSocket* socket)
{
    QString command = line.section("|", 0,0);
    if (command == "archiverecord"){
        QString station = line.section("|", 1,1);
        QString name = line.section("|",2,2);
        QString datetime = line.section("|",3, 3);
        QString value = line.section("|",4);
        QString fullname = station+"."+name;
        m_sqlstorage->archiveRecord(station, name, value+"|"+datetime, ToStorage);
        if (namesWithIntervalsTechnomer.contains(fullname))
            m_sqlstorage->roundIntervalsTechnomer(fullname, QDateTime::fromString(datetime, "yyyy-MM-dd HH:mm:ss"), value.toDouble());
        return;
    }
    if (command == "stationquery"){
        QString station = line.section("|", 1,1);
        QString query = line.section("|",2);
        QList<QHash<QString, QString> > ret = m_sqlstorage->runQuery(station, query);
        QByteArray byteArray;
        QDataStream out(&byteArray, QIODevice::WriteOnly);
        out << quint32(0) << ret;
        out.device()->seek(0);
        out << (quint32)(byteArray.size() - sizeof(quint32));
        socket->write(byteArray);
        return;
    }
    if (command == "vns3_Pres"){
        QString pres = line.section("|", 1);
        QString user = m_supervisorServer->userBySocket(socket);
        if (m_settings->getUserProp(user, "vns3_Permition") != "true"){
            m_sqlstorage->archiveRecord("vns3", "mess", m_stationNames["vns3"]+": пользователь "+user+" без прав пытался установить давление;#FFFF60", ToStorage);
            m_sqlstorage->archiveRecord("server", "mess", m_stationNames["vns3"]+": пользователь "+user+" без прав пытался установить давление;#FFFF60", ToStorage);
            return;
        }
        bool ok = false;//sendSMS("+79100067167", "выключить");
        if (ok){
            m_sqlstorage->archiveRecord("vns3", "mess", m_stationNames["vns3"]+": пользователь "+user+" успешно установил давление ("+pres+");#FFFF60", ToStorage);
            m_sqlstorage->archiveRecord("server", "mess", m_stationNames["vns3"]+": пользователь "+user+" успешно установил давление ("+pres+");#FFFF60", ToStorage);
            m_sqlstorage->archiveRecord("vns3", "settedPres", pres, OnChange);
        }
        else{
            m_sqlstorage->archiveRecord("vns3", "mess", m_stationNames["vns3"]+": пользователь "+user+" неуспешно пытался установить давление;#FFFF60", ToStorage);
            m_sqlstorage->archiveRecord("server", "mess", m_stationNames["vns3"]+": пользователь "+user+" неуспешно пытался установить давление;#FFFF60", ToStorage);
        }
        return;
    }
    if (command == "vns4_Off"){
        QString user = m_supervisorServer->userBySocket(socket);
        if (m_settings->getUserProp(user, "vns4_Permition") != "true"){
            m_sqlstorage->archiveRecord("vns4", "mess", m_stationNames["vns4"]+": пользователь "+user+" без прав пытался выключить станцию;#FFFF60", ToStorage);
            m_sqlstorage->archiveRecord("server", "mess", m_stationNames["vns4"]+": пользователь "+user+" без прав пытался выключить станцию;#FFFF60", ToStorage);
            return;
        }
        bool ok = sendSMS("+79873982924", "OFF");
        if (ok){
            m_sqlstorage->archiveRecord("vns4", "mess", m_stationNames["vns4"]+": пользователь "+user+" успешно выключил станцию;#FFFF60", ToStorage);
            m_sqlstorage->archiveRecord("server", "mess", m_stationNames["vns4"]+": пользователь "+user+" успешно выключил станцию;#FFFF60", ToStorage);
        }else{
            m_sqlstorage->archiveRecord("vns4", "mess", m_stationNames["vns4"]+": пользователь "+user+" неуспешно пытался выключить станцию;#FFFF60", ToStorage);
            m_sqlstorage->archiveRecord("server", "mess", m_stationNames["vns4"]+": пользователь "+user+" неуспешно пытался выключить станцию;#FFFF60", ToStorage);
        }
        return;
    }
    if (command == "vns5_Off"){
        QString user = m_supervisorServer->userBySocket(socket);
        if (m_settings->getUserProp(user, "vns5_Permition") != "true"){
            m_sqlstorage->archiveRecord("vns5", "mess", m_stationNames["vns5"]+": пользователь "+user+" без прав пытался выключить станцию;#FFFF60", ToStorage);
            m_sqlstorage->archiveRecord("server", "mess", m_stationNames["vns5"]+": пользователь "+user+" без прав пытался выключить станцию;#FFFF60", ToStorage);
            return;
        }
        bool ok = sendSMS("+79100064114", "OFF");
        if (ok){
            m_sqlstorage->archiveRecord("vns5", "mess", m_stationNames["vns5"]+": пользователь "+user+" успешно выключил станцию;#FFFF60", ToStorage);
            m_sqlstorage->archiveRecord("server", "mess", m_stationNames["vns5"]+": пользователь "+user+" успешно выключил станцию;#FFFF60", ToStorage);
        } else {
            m_sqlstorage->archiveRecord("vns5", "mess", m_stationNames["vns5"]+": пользователь "+user+" неуспешно пытался выключить станцию;#FFFF60", ToStorage);
            m_sqlstorage->archiveRecord("server", "mess", m_stationNames["vns5"]+": пользователь "+user+" неуспешно пытался выключить станцию;#FFFF60", ToStorage);
        }
        return;
    }
    if (command == "errorReceived"){
        QString station = line.section("|", 1, 1);
        QString state = line.section("|", 2, 2);
        QString user = m_supervisorServer->userBySocket(socket);
        if (m_settings->getUserProp(user, "confirmPermition") != "true"){
            m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": пользователь "+user+" без прав пытался подтвердить ошибку;#FFFF60", ToStorage);
            m_sqlstorage->archiveRecord("server", "mess", m_stationNames[station]+": пользователь "+user+" без прав пытался подтвердить ошибку;#FFFF60", ToStorage);
            return;
        }
        if (m_sqlstorage->isSame(station, state, "2")){
            m_sqlstorage->archiveRecord(station, state, "1");
            m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": пользователь "+user+" подтвердил состояние "+m_stateStrings[state]+";#FFFF60", ToStorage);
            m_sqlstorage->archiveRecord("server", "mess", m_stationNames[station]+": пользователь "+user+" подтвердил состояние "+m_stateStrings[state]+";#FFFF60", ToStorage);
        }
        return;
    }
    if (command == "wantedRegisters"){
        QString s_registers = line.section("|", 1);
        QStringList registers = s_registers.split("|");
        foreach(QString reg, registers){
            QString station = reg.section(".", 0,0);
            QString name = reg.section(".", 1);
            m_socketRegisters[socket].insert(reg);
            QList<CSRecord> rec = m_sqlstorage->getRecords(station, name);
            if (!rec.empty()){
                QString fullValue = rec.first().datetimeToString();
                fullValue += "|"+rec.first().value;
                sendReg(socket, reg, fullValue);
            }
        }
        return;
    }
    if (command == "wantedValue") {
        //то же, что wantedRegisters, но однократно отсылает только последнее значение (без подписки на обновления)
        QString s_registers = line.section("|", 1);
        QStringList registers = s_registers.split("|");
        foreach(QString reg, registers){
            QString station = reg.section(".", 0,0);
            QString name = reg.section(".", 1);
            QList<CSRecord> rec = m_sqlstorage->getRecords(station, name);
            if (!rec.empty()){
                QString fullValue = rec.first().datetimeToString();
                fullValue += "|"+rec.first().value;
                sendReg(socket, reg, fullValue);
            }
        }
        return;
    }
    if (command == "requestStation") {
        QString station = line.section("|", 1);
        m_mainWindow.setLogLine(QString("Запрошен принудительный опрос станции %1").arg(m_stationNames.contains(station)?m_stationNames.value(station):station), MainWindow::LogTarget::ALL);
        requestVNS(station);
    }
    if (command == "switchOnBore"){
        QString boreId = line.section("|", 1);
        if (m_boreFatekCollectors.contains(boreId)){
            QString user = m_supervisorServer->userBySocket(socket);
            if (m_settings->getUserProp(user, "switchBorePermition") != "true"){
                m_sqlstorage->archiveRecord("bore"+boreId, "mess", m_stationNames["bore"+boreId]+": пользователь "+user+" без прав пытался включить скважину;#FFFF60", ToStorage);
                return;
            }
            m_boreFatekCollectors[boreId]->sendRegister(boreId.toInt(), 8, 1);
            QString mess = m_supervisorServer->userBySocket(socket) + ": включить насос скважины " + boreId;
            m_sqlstorage->archiveRecord("bore"+boreId, "mess", mess, ToStorage);
            m_sqlstorage->archiveRecord("server", "mess", mess, ToStorage);
        } else {
            m_mainWindow.setLogLine("Нельзя включить неизвестную скважину", MainWindow::LogTarget::WINDOW);
        }
        return;
    }
    if (command == "switchOffBore"){
        QString boreId = line.section("|", 1);
        if (m_boreFatekCollectors.contains(boreId)){
            QString user = m_supervisorServer->userBySocket(socket);
            if (m_settings->getUserProp(user, "switchBorePermition") != "true"){
                m_sqlstorage->archiveRecord("bore"+boreId, "mess", m_stationNames["bore"+boreId]+": пользователь "+user+" без прав пытался выключить скважину;#FFFF60", ToStorage);
                return;
            }
            m_boreFatekCollectors[boreId]->sendRegister(boreId.toInt(), 8, 0);
            QString mess = m_supervisorServer->userBySocket(socket) + ": выключить насос скважины " + boreId;
            m_sqlstorage->archiveRecord("bore"+boreId, "mess", mess, ToStorage);
            m_sqlstorage->archiveRecord("server", "mess", mess, ToStorage);
        } else {
            m_mainWindow.setLogLine("Нельзя выключить неизвестную скважину", MainWindow::LogTarget::WINDOW);
        }
        return;
    }
    if (command == "setPressure") {
        QString station = line.section("|", 1, 1);
        QString value = line.section("|", 2);
        m_mainWindow.setLogLine(QString("Запрошена запись давления для станции %1: %2 бар").arg(m_stationNames.contains(station)?m_stationNames.value(station):station).arg(value), MainWindow::LogTarget::ALL);
        setPressure(station, value);
    }
    if (command == "setOverwork") {
        QString station = line.section("|", 1, 1);
        QString value = line.section("|", 2);
        m_mainWindow.setLogLine(QString("Запрошена запись времени переработки насоса для станции %1: %2").arg(m_stationNames.contains(station)?m_stationNames.value(station):station).arg(value), MainWindow::LogTarget::ALL);
        setKNSOverworkTime(station, value);
    }
    if (command == "setInactivity") {
        QString station = line.section("|", 1, 1);
        QString value = line.section("|", 2);
        m_mainWindow.setLogLine(QString("Запрошена запись времени бездействия насоса для станции %1: %2").arg(m_stationNames.contains(station)?m_stationNames.value(station):station).arg(value), MainWindow::LogTarget::ALL);
        setKNSInactivityTime(station, value);
    }
}

void Server::timerEvent(QTimerEvent *)
{
    QStringList stations;
    stations.append("vns1");
    stations.append("vns2");
    stations.append("vns3");
    stations.append("vns4");
    stations.append("vns5");
    stations.append("vns6");
    stations.append("vns7");
    stations.append("vns8");
    stations.append("_bs");
    stations.append("kns1");
    stations.append("kns3");
    stations.append("kns4");
    stations.append("kns5");
    stations.append("kns6");
    stations.append("kns7");
    stations.append("kns8");
    stations.append("kns9");
    stations.append("kns10");
    stations.append("kns11");
    stations.append("kns12");
    stations.append("kns13");
    stations.append("kns14");
    stations.append("kns15");
    stations.append("kns16");
    stations.append("kns17");
    stations.append("kns18");
    stations.append("bore1");
    stations.append("bore2");
    stations.append("bore3");
    stations.append("bore4");
    stations.append("bore5");
    stations.append("bore6");
    stations.append("bore7");
    stations.append("bore8");
    stations.append("bore9");
    stations.append("bore10");
    stations.append("bore11");
    stations.append("bore12");
    stations.append("bore13");
    stations.append("bore14");
    stations.append("bore16");
    stations.append("bore28");

    QMap<QString, int> linkOut; // уставки потери связи
    foreach(QString station, stations){
        if (!m_settings->contains("linkOut/"+station)){
            m_settings->setValue("linkOut/"+station, 15*60);
        }
        linkOut.insert(station, m_settings->value("linkOut/"+station).toInt());
    }

    QMap<QString, int> noneTimes; // уставки бездействия насосов
    noneTimes.insert("vns2", 30000);
    noneTimes.insert("vns6", 30000);
//    noneTimes.insert("kns1", 30000);
//    noneTimes.insert("kns3", 90000);
//    noneTimes.insert("kns4", 90000);
//    noneTimes.insert("kns5", 240000);
//    noneTimes.insert("kns6", 240000);
//    noneTimes.insert("kns7", 90000);
//    noneTimes.insert("kns8", 180000);
//    noneTimes.insert("kns9", 240000);
//    noneTimes.insert("kns10", 180000);
//    noneTimes.insert("kns11", 180000);
//    noneTimes.insert("kns12", 180000);
//    noneTimes.insert("kns13", 240000);
//    noneTimes.insert("kns14", 360000);
//    noneTimes.insert("kns15", 240000);
//    noneTimes.insert("kns16", 240000);
    noneTimes.insert("kosk", 30000);
    foreach(QString station, noneTimes.keys()){
        if (!m_settings->contains("pumpOut/"+station)){
            m_settings->setValue("pumpOut/"+station, noneTimes[station]);
        }
        noneTimes.insert(station, m_settings->value("pumpOut/"+station).toInt());
    }

    foreach(QString station, stations){
        QDateTime lastConnection = m_startingTime;
        if (!m_sqlstorage->isEmpty(station, "lastConnection")){
            lastConnection = m_sqlstorage->getRecords(station, "lastConnection").first().datetime;
        }
        if (lastConnection.addSecs(linkOut[station]) < QDateTime::currentDateTime()){
            if (m_sqlstorage->isSame(station, "linkState", "0")){
                m_sqlstorage->archiveRecord(station, "linkState", "2");
                m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": потеря связи;#FF6060", ToStorage);
            }
        }
        if (!noneTimes.contains(station))
            continue;
        QDateTime lastPumpWorking;
        lastPumpWorking = m_startingTime;
        bool switchOff = m_sqlstorage->isSame(station, "noneSignal", "0");
        if (switchOff)
            lastPumpWorking = m_sqlstorage->getRecords(station, "noneSignal").first().datetime;
        if (m_sqlstorage->isEmpty(station, "noneSignal") || switchOff){
            if (lastPumpWorking.addSecs(noneTimes[station]) < QDateTime::currentDateTime()){
                m_sqlstorage->archiveRecord(station, "noneState", "2");
                if (station == "kosk")
                    m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие отстойников;#FF6060", ToStorage);
                else
                    m_sqlstorage->archiveRecord(station, "mess", m_stationNames[station]+": бездействие насосов;#FF6060", ToStorage);
            }
        }
    }
}

bool Server::sendSMS(const QString &number, const QString &text)
{
    if (m_settings->contains("nosms") && m_settings->value("nosms").toString() == "true")
        return false;
    m_sqlstorage->archiveRecord("server", "sms", number+" : "+text, ToStorage);
    QSerialPortInfo spi;
    QString portName;
    QList<QSerialPortInfo> lsp =  QSerialPortInfo::availablePorts();
    for(QList<QSerialPortInfo>::iterator i = lsp.begin(); i != lsp.end(); i++){
        portName = i->portName();
        if (portName == "COM5"){
            spi = *i;
            break;
        }
    }
    QSerialPort sp(spi);
    QString failMsg = "Модем не подклбючен к порту COM5";
    while (portName == "COM5"){
        if (!sp.open(QIODevice::ReadWrite)){
            failMsg = "Невозможно подключиться к модему на порту COM5";
            break;
        }
        char resp[1000];
        QString resps;
        int k;
        sp.write("ATE0\r");
        if (!sp.waitForBytesWritten(5000)) {
            failMsg = "Ошибка записи команды ATE0";
            break;
        }
        resps = "";
        while(true){
            if (!sp.waitForReadyRead(100)) {
                failMsg = "Ошибка ожидания ответа на команду ATE0";
                break;
            }
            k = sp.read(resp, 999);
            if (k <= 0) {
                failMsg = "Ошибка чтения ответа на команду ATE0";
                break;
            }
            resps += QString::fromLatin1(resp, k);
        }
        resps = resps.simplified();
        if (resps != "" && resps != "OK") {
            failMsg = "Неправильный ответ на команду ATE0: " + resps;
            break;
        }
        /*sp.write("AT+CSQ\r");
        if (!sp.waitForBytesWritten(5000))
            break;
        resps = "";
        while(true){
            if (!sp.waitForReadyRead(1000))
                break;
            k = sp.read(resp, 999);
            if (k <= 0)
                break;
            resps += QString::fromLatin1(resp, k);
        }
        resps = resps.simplified();
        int level = resps.section(":", 1).section(",", 0,0).toInt();
        if (level == 0 || level == 99)
            break;
            */
        sp.write("AT+CMGF=1\r");
        if (!sp.waitForBytesWritten(5000)) {
            failMsg = "Ошибка записи команды AT+CMGF=1";
            break;
        }
        resps = "";
        while(true){
            if (!sp.waitForReadyRead(100)) {
                failMsg = "Ошибка ожидания ответа на команду AT+CMGF=1";
                break;
            }
            k = sp.read(resp, 999);
            if (k <= 0) {
                failMsg = "Ошибка чтения ответа на команду AT+CMGF=1";
                break;
            }
            resps += QString::fromLatin1(resp, k);
        }
        resps = resps.simplified();
        if (resps != "OK") {
            failMsg = "Неправильный ответ на команду ATE0: " + resps;
            break;
        }
        sp.write(("AT+CMGS=\""+number+"\"\r").toStdString().c_str());
        if (!sp.waitForBytesWritten(5000)) {
            failMsg = "Ошибка записи команды AT+CMGS";
            break;
        }
        resps = "";
        while(true){
            if (!sp.waitForReadyRead(100)) {
                failMsg = "Ошибка ожидания ответа на команду AT+CMGS";
                break;
            }
            k = sp.read(resp, 999);
            if (k <= 0) {
                failMsg = "Ошибка чтения ответа на команду AT+CMGS";
                break;
            }
            resps += QString::fromLatin1(resp, k);
        }
        QString ltext = text;
        sp.write(ltext.append(QChar(26)).toStdString().c_str());
        if (!sp.waitForBytesWritten(5000)) {
            failMsg = "Ошибка записи тела сообщения SMS";
            break;
        }
        sp.waitForReadyRead(100);
        sp.close();
        return true;
    }
    sp.close();
    m_mainWindow.setLogLine(QString("Не удалась отправка SMS: %1").arg(failMsg), MainWindow::LogTarget::ALL);
    return false;
}


