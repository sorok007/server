#include <QApplication>
#include "server.h"
#include "3rdParty/SimpleQtLogger/simpleQtLogger.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    simpleqtlogger::SimpleQtLogger::createInstance(qApp)->setLogFileName("scada_server.log", 10*1000*1024, 3);
    simpleqtlogger::SimpleQtLogger::getInstance()->setLogFormat_file("<TS> [<LL>] <TEXT>", simpleqtlogger::DEFAULT_LOG_FORMAT_INTERNAL);

    simpleqtlogger::ENABLE_LOG_SINK_FILE = true;
    simpleqtlogger::ENABLE_LOG_SINK_CONSOLE = false;
    simpleqtlogger::ENABLE_LOG_SINK_QDEBUG = false;
    simpleqtlogger::ENABLE_LOG_SINK_SIGNAL = false;

    simpleqtlogger::ENABLE_FUNCTION_STACK_TRACE = false;

    simpleqtlogger::EnableLogLevels enableLogLevels_file = simpleqtlogger::ENABLE_LOG_LEVELS;
    simpleqtlogger::SimpleQtLogger::getInstance()->setLogLevels_file(enableLogLevels_file);

    Server server;

    return a.exec();
}
