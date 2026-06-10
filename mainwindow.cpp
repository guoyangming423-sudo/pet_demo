#include "mainwindow.h"
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    // 窗口设置（和你风格一致）
    setFixedSize(480, 480);
    setWindowFlags(Qt::FramelessWindowHint);

    // 初始化状态
    m_startX = 0;
    m_isDrag = false;
    m_page = 0;

    // 滑动容器（两个界面并排：总宽度 960）
    m_slideContainer = new QWidget(this);
    m_slideContainer->setFixedSize(960, 480);

    // 创建你的两个界面
    m_watchFace = new WatchFace(m_slideContainer);
    m_watchFace->setFixedSize(480, 480);
    m_watchFace->move(0, 0);

    m_launcher = new Launcher(m_slideContainer);
    m_launcher->setFixedSize(480, 480);
    m_launcher->move(480, 0);

    // 滑动动画
    m_anim = new QPropertyAnimation(m_slideContainer, "pos", this);
    m_anim->setDuration(300);

    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);
    mainLayout->addWidget(m_slideContainer);
    setLayout(mainLayout);
}

// 触摸按下
void MainWindow::mousePressEvent(QMouseEvent *event)
{
    m_startX = event->x();
    m_isDrag = true;
}

// 触摸拖动
void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if(!m_isDrag) return;

    int dx = event->x() - m_startX;
    int targetX = -m_page * 480 + dx;

    // 限制边界
    if(targetX > 0) targetX = 0;
    if(targetX < -480) targetX = -480;

    m_slideContainer->move(targetX, 0);
}

// 触摸松开，判断切换页面
void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    m_isDrag = false;
    int dx = event->x() - m_startX;

    // 向右滑动 → 封面切组件
    if(dx < -160 && m_page == 0){
        m_anim->setStartValue(m_slideContainer->pos());
        m_anim->setEndValue(QPoint(-480, 0));
        m_anim->start();
        m_page = 1;
    }
    // 向左滑动 → 组件切封面
    else if(dx > 160 && m_page == 1){
        m_anim->setStartValue(m_slideContainer->pos());
        m_anim->setEndValue(QPoint(0, 0));
        m_anim->start();
        m_page = 0;
    }
    // 回弹
    else{
        m_anim->setStartValue(m_slideContainer->pos());
        m_anim->setEndValue(QPoint(-m_page * 480, 0));
        m_anim->start();
    }
}
