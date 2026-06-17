#ifndef CALENDAR_H
#define CALENDAR_H

#include <QWidget>
#include <QLabel>
#include <QGridLayout>
#include <QDate>

class Calendar : public QWidget
{
    Q_OBJECT
public:
    explicit Calendar(QWidget *parent = nullptr);

private slots:
    void onBackClicked();
    void onPrevMonth();  // 上月
    void onNextMonth();  // 下月

private:
    void initUI();
    void renderMonth();
    QString getSolarFestival(int month, int day);  // 获取公历节日
    QString getLunarFestival(int year, int month, int day); // 获取农历节日
    QString solarToLunar(int year, int month, int day);    // 公历转农历

    QLabel *m_titleLabel;
    QGridLayout *m_dateGrid;
    int m_year;
    int m_month;
    int m_todayDay;

    // 1900-2100年农历数据表（压缩存储，离线算法核心）
    static const unsigned int m_lunarTable[200];
};

#endif // CALENDAR_H
