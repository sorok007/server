#ifndef SMSFORM_H
#define SMSFORM_H

#include <QWidget>

namespace Ui {
class SMSForm;
}

class SMSForm : public QWidget
{
    Q_OBJECT

public:
    explicit SMSForm(QWidget *parent = 0);
    ~SMSForm();

public:
    Ui::SMSForm *ui;
};

#endif // SMSFORM_H
