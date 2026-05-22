#include "power.h"
#include <QPainter>

CircularProgress::CircularProgress(QWidget *parent)
    : QWidget(parent), m_value(88)
{
    setFixedSize(60, 60);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void CircularProgress::setValue(int value)
{
    m_value = qBound(0, value, 100);
    update();
}

void CircularProgress::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::transparent);

    int w = width();
    int h = height();
    int side = qMin(w, h);
    // 居中绘制，留足够边距
    QRectF rect(5, 5, side - 10, side - 10);

    // 1. 背景圆环
    QPen bgPen(QColor(80, 180, 80, 80), 6);
    bgPen.setCapStyle(Qt::RoundCap); // 圆角线头，更美观
    p.setPen(bgPen);
    p.drawEllipse(rect);

    // 2. 进度圆环
    QPen progressPen(QColor(50, 220, 50), 6);
    progressPen.setCapStyle(Qt::RoundCap);
    p.setPen(progressPen);
    int angle = 360 * m_value / 100;
    p.drawArc(rect, 90 * 16, -angle * 16); // 从顶部顺时针绘制

    // 3. 中间文字
    p.setPen(Qt::white);
    QFont font;
    font.setPixelSize(14);
    font.setBold(true);
    p.setFont(font);
    p.drawText(rect, Qt::AlignCenter, QString::number(m_value) + "%");
}
