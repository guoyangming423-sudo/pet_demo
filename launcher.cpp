#include "launcher.h"
#include <QGridLayout>
#include <QLabel>
#include <QPixmap>
#include <QDebug>

Launcher::Launcher(QWidget *parent)
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

    // ===== 2. 九宫格布局 =====
    QGridLayout *gridLayout = new QGridLayout(bgLabel);
    gridLayout->setAlignment(Qt::AlignCenter);
    gridLayout->setContentsMargins(40, 40, 40, 40);
    gridLayout->setSpacing(35);

    // ===== 3. 创建应用按钮 =====
    phoneBtn = createAppButton("/root/icon/phone-green.png", "电话");
    messagesBtn = createAppButton("/root/icon/Messages.png", "信息");
    cameraBtn = createAppButton("/root/icon/Camera.png", "相机");
    calendarBtn = createAppButton("/root/icon/Calendar.png", "日历");
    clockBtn = createAppButton("/root/icon/Clock.png", "时钟");
    calculatorBtn = createAppButton("/root/icon/Calculator.png", "计算器");
    photosBtn = createAppButton("/root/icon/Photos.png", "相册");
    musicBtn = createAppButton("/root/icon/Music.png", "音乐");
    settingsBtn = createAppButton("/root/icon/Settings.png", "设置");

    // ===== 4. 添加到九宫格 =====
    gridLayout->addWidget(phoneBtn, 0, 0);
    gridLayout->addWidget(messagesBtn, 0, 1);
    gridLayout->addWidget(cameraBtn, 0, 2);
    gridLayout->addWidget(calendarBtn, 1, 0);
    gridLayout->addWidget(clockBtn, 1, 1);
    gridLayout->addWidget(calculatorBtn, 1, 2);
    gridLayout->addWidget(photosBtn, 2, 0);
    gridLayout->addWidget(musicBtn, 2, 1);
    gridLayout->addWidget(settingsBtn, 2, 2);

    // ===== 5. 连接信号槽 =====
    connect(phoneBtn, &QPushButton::clicked, this, &Launcher::onPhoneClicked);
    connect(messagesBtn, &QPushButton::clicked, this, &Launcher::onMessagesClicked);
    connect(cameraBtn, &QPushButton::clicked, this, &Launcher::onCameraClicked);
    connect(calendarBtn, &QPushButton::clicked, this, &Launcher::onCalendarClicked);
    connect(clockBtn, &QPushButton::clicked, this, &Launcher::onClockClicked);
    connect(calculatorBtn, &QPushButton::clicked, this, &Launcher::onCalculatorClicked);
    connect(photosBtn, &QPushButton::clicked, this, &Launcher::onPhotosClicked);
    connect(musicBtn, &QPushButton::clicked, this, &Launcher::onMusicClicked);
    connect(settingsBtn, &QPushButton::clicked, this, &Launcher::onSettingsClicked);

    // ===== 6. 设置主布局（关键）=====
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);  // 去边距
    mainLayout->addWidget(bgLabel);

    setLayout(mainLayout);
}

// 创建应用按钮的辅助函数
QPushButton* Launcher::createAppButton(const QString& iconPath, const QString& text)
{
    QPushButton *btn = new QPushButton(this);
    btn->setFixedSize(85, 105);
    btn->setFlat(true);

    // 手动创建图标+文字布局（替代setToolButtonStyle）
    QVBoxLayout *btnLayout = new QVBoxLayout(btn);
    btnLayout->setAlignment(Qt::AlignCenter);
    btnLayout->setSpacing(8);
    btnLayout->setContentsMargins(0, 0, 0, 0);

    // 图标
    QLabel *iconLabel = new QLabel(btn);
    QPixmap icon(iconPath);
    iconLabel->setPixmap(icon.scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setAlignment(Qt::AlignCenter);

    // 文字
    QLabel *textLabel = new QLabel(text, btn);
    textLabel->setAlignment(Qt::AlignCenter);
    textLabel->setStyleSheet("color: white; font-size: 17px; font-weight: bold;");

    // 组装按钮
    btnLayout->addWidget(iconLabel);
    btnLayout->addWidget(textLabel);

    // 按钮样式
    btn->setStyleSheet(R"(
        QPushButton {
            border: none;
            background-color: transparent;
        }
        QPushButton:pressed {
            background-color: rgba(255, 255, 255, 30);
            border-radius: 42px;
        }
    )");

    return btn;
}

// 按钮点击事件处理
void Launcher::onPhoneClicked() { qDebug() << "电话被点击"; }
void Launcher::onMessagesClicked() { qDebug() << "信息被点击"; }
void Launcher::onCameraClicked() { qDebug() << "相机被点击"; }
void Launcher::onCalendarClicked() { qDebug() << "日历被点击"; }
void Launcher::onClockClicked() { qDebug() << "时钟被点击"; }
void Launcher::onCalculatorClicked()
{
    qDebug() << "计算器被点击";
    AdvancedCalculator *calc = new AdvancedCalculator(this);
    calc->show();
    calc->setAttribute(Qt::WA_DeleteOnClose); // 关闭自动释放内存
}
void Launcher::onPhotosClicked() { qDebug() << "相册被点击"; }
void Launcher::onMusicClicked() { qDebug() << "音乐被点击"; }
void Launcher::onSettingsClicked() { qDebug() << "设置被点击"; }
