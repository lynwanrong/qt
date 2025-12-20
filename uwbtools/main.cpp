#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    // High DPI scaling support for Qt 5.14+
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
