#include <QApplication>
#include <QLocale>
#include <QFont>
#include "watchface.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QLocale::setDefault(QLocale::Chinese);
    QFont font("Source Han Sans CN");
    
    WatchFace w;
    w.showFullScreen();   // 关键：手表模式全屏

    return app.exec();
}
