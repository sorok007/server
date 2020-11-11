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
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    Ui::MainWindow* getUI() { return ui; }

public slots:
    void setLogLine(const QString &log);

public:
    Ui::MainWindow *ui;
    Bore* bore;
    SMSForm* smsForm;
};

#endif // MAINWINDOW_H
