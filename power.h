#ifndef POWER_H
#define POWER_H

#include <QWidget>

class CircularProgress : public QWidget
{
    Q_OBJECT
    
public:
    explicit CircularProgress(QWidget *parent = nullptr);

    void setValue(int value);  // 0~100

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_value;
};

#endif
