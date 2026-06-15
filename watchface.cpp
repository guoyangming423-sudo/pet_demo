#include "watchface.h"
#include "power.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QDateTime>
#include <QFont>

WatchFace::WatchFace(QWidget *parent)
    : QWidget(parent)
{
    // ===== 1. 背景容器（核心）=====
    QLabel *bgLabel = new QLabel(this);

    QPixmap bg("/root/icon/bg.jpg");
    bg = bg.scaled(480, 480,
                   Qt::KeepAspectRatioByExpanding,
                   Qt::SmoothTransformation);

    bgLabel->setPixmap(bg);
    bgLabel->setAlignment(Qt::AlignCenter);

    // ===== 2. UI布局 =====
    QVBoxLayout *layout = new QVBoxLayout(bgLabel);
    layout->setAlignment(Qt::AlignCenter);

    layout->setContentsMargins(0, 50, 0, 40);

    // ===== 3. 日期 =====
    dateLabel = new QLabel(bgLabel);
    dateLabel->setStyleSheet("color:white; font-size:18px; font-weight: 600;");
    dateLabel->setAlignment(Qt::AlignCenter);

    // ===== 定位模块 =====
    QWidget *locationCard = new QWidget(bgLabel);
    locationCard->setStyleSheet(R"(
        QWidget {
            background-color: rgba(0, 0, 0, 100);
            border-radius: 15px;
            border: 1px solid rgba(255,255,255,80);
        }
        QLabel {
            background: transparent;
            border: none;
        }
    )");
    locationCard->setFixedSize(90, 40); // 卡片大小

    QHBoxLayout *locationLayout = new QHBoxLayout(locationCard);
    locationLayout->setAlignment(Qt::AlignCenter);
    locationLayout->setSpacing(3);
    locationLayout->setContentsMargins(8, 3, 10, 5);

    QLabel *locationIcon = new QLabel(locationCard);
    locationIcon->setPixmap(QPixmap("/root/icon/location.png").scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QLabel *locationText = new QLabel("佛山", locationCard);
    locationText->setStyleSheet("color:white; font-size:20px; font-weight:600;");

    locationLayout->addWidget(locationIcon);
    locationLayout->addWidget(locationText);

    // ===== 4. 时间 =====
    timeLabel = new QLabel(bgLabel);
    QFont timeFont("Arial", 60, QFont::Bold);
    timeLabel->setFont(timeFont);
    timeLabel->setStyleSheet("color:white;");
    timeLabel->setAlignment(Qt::AlignCenter);

    // ===== 天气卡片 =====
    QWidget *weatherCard = new QWidget(bgLabel);

    weatherCard->setStyleSheet(R"(
        QWidget {
            background-color: rgba(0, 0, 0, 100);
            border-radius: 15px;
            border: 1px solid rgba(255,255,255,80);
        }
        QLabel {
            background: transparent;
            border: none;
        }
    )");

    // 固定卡片大小
    weatherCard->setFixedSize(160, 60);

    QHBoxLayout *weatherLayout = new QHBoxLayout(weatherCard);
    weatherLayout->setContentsMargins(15, 8, 15, 8);
    weatherLayout->setSpacing(10);
    weatherLayout->setAlignment(Qt::AlignCenter);

    // icon
    QLabel *weatherIcon = new QLabel(weatherCard);
    weatherIcon->setPixmap(QPixmap("/root/icon/weather.png").scaled(35, 35, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // text
    QLabel *weatherText = new QLabel("19°C", weatherCard);
    weatherText->setStyleSheet("color:#00d4ff; font-size:24px; font-weight:600;");

    weatherLayout->addWidget(weatherIcon);
    weatherLayout->addWidget(weatherText);

    // ===== 圆形进度 + 心率 =====
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(80, 0, 60, 0);
    bottomLayout->setSpacing(80);
    bottomLayout->setAlignment(Qt::AlignCenter);

    // 圆形进度
    CircularProgress *progress = new CircularProgress(bgLabel);
    progress->setValue(88);

    // 心率
    QHBoxLayout *heartLayout = new QHBoxLayout();
    heartLayout->setSpacing(5);
    heartLayout->setAlignment(Qt::AlignCenter);

    QLabel *heartIcon = new QLabel(bgLabel);
    QPixmap hicon("/root/icon/heart.png");
    heartIcon->setPixmap(hicon.scaled(30, 30));

    QLabel *heartText = new QLabel("73 BPM", bgLabel);
    heartText->setStyleSheet("color:white; font-size:20px;");

    heartLayout->addWidget(heartIcon);
    heartLayout->addWidget(heartText);

    bottomLayout->addWidget(progress, 0, Qt::AlignLeft);
    //bottomLayout->addStretch();
    bottomLayout->addLayout(heartLayout);


    // ===== 7. 加入布局 =====
    layout->addWidget(dateLabel);
    layout->addSpacing(5);

    layout->addWidget(locationCard, 0, Qt::AlignCenter);
    layout->addSpacing(15);

    layout->addWidget(timeLabel);
    layout->addSpacing(15);

    layout->addWidget(weatherCard, 0, Qt::AlignCenter);
    layout->addSpacing(25);

    layout->addLayout(bottomLayout);

    // ===== 8. 设置主布局（关键）=====
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);  // 去边距
    mainLayout->addWidget(bgLabel);

    setLayout(mainLayout);

    // ===== 9. 定时器 =====
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &WatchFace::updateTime);
    timer->start(1000);

    updateTime();
}

void WatchFace::updateTime()
{
    QDateTime now = QDateTime::currentDateTime();

    dateLabel->setText(now.toString("MM月dd日  dddd"));
    timeLabel->setText(now.toString("hh:mm"));
}
