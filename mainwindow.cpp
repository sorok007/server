#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    bore = new Bore();
    connect(ui->boreAction, SIGNAL(triggered(bool)), this->bore, SLOT(show()));
    smsForm = new SMSForm();
    connect(ui->smsAction, SIGNAL(triggered(bool)), this->smsForm, SLOT(show()));
}

MainWindow::~MainWindow()
{
    delete ui;
    delete bore;
    delete smsForm;
}

void MainWindow::setLogLine(const QString &log)
{
    static quint64 n = 1;
    static QStringList textLog;
    QString nlog = log;
    textLog.prepend(nlog.prepend(QString::number(n++)+") "));
    if (textLog.size() > 100)
        textLog.removeLast();
    QString text = textLog.join(QString("\n"));
    this->ui->textLog->setPlainText(text);
}
