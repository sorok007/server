#include "smsform.h"
#include "ui_smsform.h"

SMSForm::SMSForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SMSForm)
{
    ui->setupUi(this);
}

SMSForm::~SMSForm()
{
    delete ui;
}
