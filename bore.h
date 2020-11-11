#ifndef BORE_H
#define BORE_H

#include <QWidget>

namespace Ui {
class Bore;
}

class Bore : public QWidget
{
    Q_OBJECT

public:
    explicit Bore(QWidget *parent = 0);
    ~Bore();

public:
    Ui::Bore *ui;
};

#endif // BORE_H
