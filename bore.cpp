#include "bore.h"
#include "ui_bore.h"

Bore::Bore(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Bore)
{
    ui->setupUi(this);
    this->setWindowModality(Qt::ApplicationModal);
    this->setWindowFlags(Qt::Dialog);
}

Bore::~Bore()
{
    delete ui;
}
