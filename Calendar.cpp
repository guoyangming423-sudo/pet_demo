#include "Calendar.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QFont>
#include <QVBoxLayout>
#include <QRegularExpression>

// 农历数据表：1900-2099年，每年一个整数，存闰月、每月大小等信息
const unsigned int Calendar::m_lunarTable[200] = {
    0x04bd8,0x04ae0,0x0a570,0x054d5,0x0d260,0x0d950,0x16554,0x056a0,0x09ad0,0x055d2,
    0x04ae0,0x0a5b6,0x0a4d0,0x0d250,0x1d255,0x0b540,0x0d6a0,0x0ada2,0x095b0,0x14977,
    0x04970,0x0a4b0,0x0b4b5,0x06a50,0x06d40,0x1ab54,0x02b60,0x09570,0x052f2,0x04970,
    0x06566,0x0d4a0,0x0ea50,0x06e95,0x05ad0,0x02b60,0x186e3,0x092e0,0x1c8d7,0x0c950,
    0x0d4a0,0x1d8a6,0x0b550,0x056a0,0x1a5b4,0x025d0,0x092d0,0x0d2b2,0x0a950,0x0b557,
    0x06ca0,0x0b550,0x15355,0x04da0,0x0a5d0,0x14573,0x052d0,0x0a9a8,0x0e950,0x06aa0,
    0x0aea6,0x0ab50,0x04b60,0x0aae4,0x0a570,0x05260,0x0f263,0x0d950,0x05b57,0x056a0,
    0x096d0,0x04dd5,0x04ad0,0x0a4d0,0x0d4d4,0x0d250,0x0d558,0x0b540,0x0b5a0,0x195a6,
    0x095b0,0x049b0,0x0a974,0x0a4b0,0x0b27a,0x06a50,0x06d40,0x0af46,0x0ab60,0x09570,
    0x04af5,0x04970,0x064b0,0x074a3,0x0ea50,0x06b58,0x055c0,0x0ab60,0x096d5,0x092e0,
    0x0c960,0x0d954,0x0d4a0,0x0da50,0x07552,0x056a0,0x0abb7,0x025d0,0x092d0,0x0cab5,
    0x0a950,0x0b4a0,0x0baa4,0x0ad50,0x055d9,0x04ba0,0x0a5b0,0x15176,0x052b0,0x0a930,
    0x07954,0x06aa0,0x0ad50,0x05b52,0x04b60,0x0a6e6,0x0a4e0,0x0d260,0x0ea65,0x0d530,
    0x05aa0,0x076a3,0x096d0,0x04afb,0x04ad0,0x0a4d0,0x1d0b6,0x0d250,0x0d520,0x0dd45,
    0x0b5a0,0x056d0,0x055b2,0x049b0,0x0a577,0x0a4b0,0x0aa50,0x1b255,0x06d20,0x0ada0
};

Calendar::Calendar(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint);
    setFixedSize(480, 480);
    setWindowModality(Qt::ApplicationModal);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background-color: #000000;");

    QDate today = QDate::currentDate();
    m_year = today.year();
    m_month = today.month();
    m_todayDay = today.day();

    initUI();
    renderMonth();
}

void Calendar::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. 顶部标题栏
    QWidget *titleBar = new QWidget;
    titleBar->setFixedHeight(55);
    QHBoxLayout *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(10, 0, 10, 0);

    QPushButton *backBtn = new QPushButton("返回");
    backBtn->setFixedSize(70, 32);
    backBtn->setStyleSheet("color:white; background:#333; border-radius:16px; font-size:14px;");
    connect(backBtn, &QPushButton::clicked, this, &Calendar::onBackClicked);
    titleLayout->addWidget(backBtn);

    QPushButton *prevBtn = new QPushButton("<");
    prevBtn->setFixedSize(32, 32);
    prevBtn->setStyleSheet("color:white; background:transparent; font-size:20px;");
    connect(prevBtn, &QPushButton::clicked, this, &Calendar::onPrevMonth);
    titleLayout->addWidget(prevBtn);

    m_titleLabel = new QLabel();
    m_titleLabel->setStyleSheet("color:white; font-size:20px; font-weight:bold;");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    titleLayout->addStretch();
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();

    QPushButton *nextBtn = new QPushButton(">");
    nextBtn->setFixedSize(32, 32);
    nextBtn->setStyleSheet("color:white; background:transparent; font-size:20px;");
    connect(nextBtn, &QPushButton::clicked, this, &Calendar::onNextMonth);
    titleLayout->addWidget(nextBtn);

    mainLayout->addWidget(titleBar);

    // 2. 日期网格（星期和日期都在renderMonth里生成）
    m_dateGrid = new QGridLayout();
    m_dateGrid->setContentsMargins(10, 5, 10, 5);
    m_dateGrid->setSpacing(2);
    for(int i=0; i<7; i++) m_dateGrid->setColumnStretch(i, 1);
    m_dateGrid->setRowStretch(0, 0);
    for(int i=1; i<=6; i++) m_dateGrid->setRowStretch(i, 1);

    mainLayout->addLayout(m_dateGrid);
}

void Calendar::renderMonth()
{
    // 安全清空：从后往前删除所有item，避免索引错乱
    while(m_dateGrid->count() > 0){
        QLayoutItem *item = m_dateGrid->takeAt(m_dateGrid->count() - 1);
        if(item && item->widget()){
            delete item->widget();
        }
        delete item;
    }

    m_titleLabel->setText(QString("%1年%2月").arg(m_year).arg(m_month));

    // 重新添加星期表头（第0行）
    QStringList weekTexts = {"日", "一", "二", "三", "四", "五", "六"};
    for(int i=0; i<7; i++){
        QLabel *lab = new QLabel(weekTexts[i]);
        lab->setAlignment(Qt::AlignCenter);
        lab->setStyleSheet("color:#888; font-size:14px;");
        lab->setFixedHeight(30);
        m_dateGrid->addWidget(lab, 0, i);
    }

    QDate firstDay(m_year, m_month, 1);
    int weekOfFirst = firstDay.dayOfWeek() % 7;
    int daysInMonth = firstDay.daysInMonth();

    int dayNum = 1;
    // 日期从第1行开始，共6行
    for(int row=1; row<=6; row++){
        for(int col=0; col<7; col++){
            QWidget *dayWidget = new QWidget;
            QVBoxLayout *dayLayout = new QVBoxLayout(dayWidget);
            dayLayout->setContentsMargins(0, 2, 0, 2);
            dayLayout->setSpacing(0);

            QLabel *dayLab = new QLabel();
            dayLab->setAlignment(Qt::AlignCenter);
            dayLab->setFont(QFont("", 16));
            dayLayout->addWidget(dayLab);

            QLabel *festivalLab = new QLabel();
            festivalLab->setAlignment(Qt::AlignCenter);
            festivalLab->setFont(QFont("", 9));
            dayLayout->addWidget(festivalLab);

            if(row == 1 && col < weekOfFirst){
                dayLab->setText("");
                festivalLab->setText("");
            }else if(dayNum > daysInMonth){
                dayLab->setText("");
                festivalLab->setText("");
            }else{
                dayLab->setText(QString::number(dayNum));

                QString solarFest = getSolarFestival(m_month, dayNum);
                QString lunarFest = getLunarFestival(m_year, m_month, dayNum);
                QString festival = solarFest.isEmpty() ? lunarFest : solarFest;

                if(dayNum == m_todayDay){
                    dayLab->setStyleSheet(
                        "color:white; font-weight:bold;"
                        "background-color:#ff3b30;"
                        "border-radius:16px;"
                        "min-height:32px; max-height:32px;"
                        "min-width:32px; max-width:32px;"
                        "margin:0 auto;"
                    );
                    festivalLab->setStyleSheet("color:#ff3b30;");
                }else{
                    dayLab->setStyleSheet("color:white;");
                    festivalLab->setStyleSheet("color:#ff9500;");
                }

                if(!festival.isEmpty()){
                    festivalLab->setText(festival);
                }

                dayNum++;
            }
            m_dateGrid->addWidget(dayWidget, row, col);
        }
    }
}

// 公历节日查询
QString Calendar::getSolarFestival(int month, int day)
{
    struct SolarFest { int month; int day; QString name; };
    static const SolarFest fests[] = {
        {1, 1, "元旦"}, {2, 14, "情人节"}, {3, 8, "妇女节"},
        {3, 12, "植树节"}, {4, 1, "愚人节"}, {5, 1, "劳动节"},
        {5, 4, "青年节"}, {6, 1, "儿童节"}, {7, 1, "建党节"},
        {8, 1, "建军节"}, {9, 10, "教师节"}, {10, 1, "国庆节"},
        {12, 25, "圣诞节"}
    };
    for(const auto &f : fests){
        if(f.month == month && f.day == day) return f.name;
    }
    return "";
}

// 公历转农历核心算法
QString Calendar::solarToLunar(int year, int month, int day)
{
    if(year < 1900 || year > 2099) return "";

    // 基准日期：1900年1月31日是农历正月初一
    QDate baseDate(1900, 1, 31);
    QDate solarDate(year, month, day);
    qint64 offset = baseDate.daysTo(solarDate);

    // 日期非法也直接返回
    if(offset < 0) return "";

    int lunarYear = 1900;
    int daysInYear;
    // 计算农历年份
    while(lunarYear < 2100 && offset > 0){
        daysInYear = 0;
        // 计算该农历年总天数
        for(int i=0x8000; i>0x8; i>>=1){
            daysInYear += (m_lunarTable[lunarYear-1900] & i) ? 30 : 29;
        }
        // 加闰月天数
        int leapMonth = m_lunarTable[lunarYear-1900] & 0xf;
        if(leapMonth){
            int leapDays = (m_lunarTable[lunarYear-1900] & 0x10000) ? 30 : 29;
            daysInYear += leapDays;
        }

        if(offset < daysInYear) break;
        offset -= daysInYear;
        lunarYear++;
    }

    int leapMonth = m_lunarTable[lunarYear-1900] & 0xf;
    bool isLeap = false;
    int lunarMonth = 1;
    int daysInMonth;

    // 计算农历月份
    for(int i=0x8000; i>0x8; i>>=1){
        daysInMonth = (m_lunarTable[lunarYear-1900] & i) ? 30 : 29;
        if(offset < daysInMonth) break;
        offset -= daysInMonth;
        lunarMonth++;

        // 处理闰月
        if(leapMonth && lunarMonth == leapMonth+1 && !isLeap){
            isLeap = true;
            int leapDays = (m_lunarTable[lunarYear-1900] & 0x10000) ? 30 : 29;
            if(offset < leapDays) break;
            offset -= leapDays;
            lunarMonth++;
        }
    }

    int lunarDay = offset + 1;
    return QString("%1年%2月%3").arg(lunarYear).arg(lunarMonth).arg(lunarDay);
}

// 农历节日查询
QString Calendar::getLunarFestival(int year, int month, int day)
{
    QString lunarStr = solarToLunar(year, month, day);
    if(lunarStr.isEmpty()) return "";

    // 只用一套正则解析农历月日，删掉之前无效的split拆分代码
    int lunarMonth = 0, lunarDay = 0;
    QRegularExpression re("(\\d+)年(\\d+)月(\\d+)");
    QRegularExpressionMatch match = re.match(lunarStr);
    if(match.hasMatch()){
        lunarMonth = match.captured(2).toInt();
        lunarDay = match.captured(3).toInt();
    }

    // 农历节日表
    struct LunarFest { int month; int day; QString name; };
    static const LunarFest fests[] = {
        {1, 1, "春节"}, {1, 15, "元宵节"}, {2, 2, "龙抬头"},
        {5, 5, "端午节"}, {7, 7, "七夕"}, {7, 15, "中元节"},
        {8, 15, "中秋节"}, {9, 9, "重阳节"}, {12, 8, "腊八节"},
        {12, 23, "小年"}, {12, 30, "除夕"}
    };
    // 传统for循环，兼容所有Qt编译器，不需要C++11特性
    int festTotal = sizeof(fests) / sizeof(LunarFest);
    for(int i = 0; i < festTotal; i++){
        const LunarFest &festItem = fests[i];
        if(festItem.month == lunarMonth && festItem.day == lunarDay){
            return festItem.name;
        }
    }
    return "";
}

void Calendar::onPrevMonth()
{
    m_month--;
    if(m_month < 1){
        m_month = 12;
        m_year--;
    }
    renderMonth();
}

void Calendar::onNextMonth()
{
    m_month++;
    if(m_month > 12){
        m_month = 1;
        m_year++;
    }
    renderMonth();
}

void Calendar::onBackClicked()
{
    close();
}
