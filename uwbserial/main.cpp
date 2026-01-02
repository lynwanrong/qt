#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    // 启用高分屏缩放支持
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication a(argc, argv);

    // 设置字体
    // QFont font("Microsoft YaHei", 9);
    // a.setFont(font);

    MainWindow w;
    w.show();

    return a.exec();
}
