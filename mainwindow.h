#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QPropertyAnimation>
#include "watchface.h"
#include "launcher.h"

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QWidget* m_slideContainer;    // 滑动容器
    WatchFace* m_watchFace;       // 封面界面
    Launcher* m_launcher;         // 组件界面
    QPropertyAnimation* m_anim;   // 滑动动画

    int m_startX;    // 触摸起始坐标
    bool m_isDrag;   // 是否正在拖动
    int m_page;      // 0=封面 1=组件
};

#endif // MAINWINDOW_H
