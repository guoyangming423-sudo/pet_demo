#ifndef WATCHFACE_H
#define WATCHFACE_H

#include <QWidget>
#include <QLabel>
#include <QTimer>

class WatchFace : public QWidget
{
    Q_OBJECT

public:
    explicit WatchFace(QWidget *parent = nullptr);

private slots:
    void updateTime();

private:
    QLabel *timeLabel;
    QLabel *dateLabel;

    QTimer *timer;
};

#endif
