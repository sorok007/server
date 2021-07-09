#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "bore.h"
#include "smsform.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum LogTarget {
        WINDOW = 0x1,
        FILE = 0x2,
        ALL = 0x3
    };

    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    Ui::MainWindow* getUI() { return ui; }

public slots:
    void setLogLine(const QString &log, LogTarget logTarget = LogTarget::FILE);

public:
    Ui::MainWindow *ui;
    Bore* bore;
    SMSForm* smsForm;
};

#endif // MAINWINDOW_H
