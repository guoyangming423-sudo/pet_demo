#include <QApplication>
#include <QLocale>
#include <QFont>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QLocale::setDefault(QLocale::Chinese);
    QFont font("Source Han Sans CN");

    MainWindow w;
    w.showFullScreen();

    return app.exec();
}
