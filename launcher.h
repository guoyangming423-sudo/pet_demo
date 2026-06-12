#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <QWidget>
#include <QPushButton>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <cmath>
#include "Calculator.h"

class Launcher : public QWidget
{
    Q_OBJECT

public:
    explicit Launcher(QWidget *parent = nullptr);

private slots:
    void onPhoneClicked();
    void onMessagesClicked();
    void onCameraClicked();
    void onCalendarClicked();
    void onClockClicked();
    void onCalculatorClicked();
    void onPhotosClicked();
    void onMusicClicked();
    void onSettingsClicked();

private:
    // 辅助函数声明（修复的关键）
    QPushButton* createAppButton(const QString& iconPath, const QString& text);

    QPushButton *phoneBtn;
    QPushButton *messagesBtn;
    QPushButton *cameraBtn;
    QPushButton *calendarBtn;
    QPushButton *clockBtn;
    QPushButton *calculatorBtn;
    QPushButton *photosBtn;
    QPushButton *musicBtn;
    QPushButton *settingsBtn;
};

#endif // LAUNCHER_H
