#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 创建并显示主窗口
    // MainWindow 的构造函数里会自动加载数据并画图
    MainWindow w;
    w.show();

    return a.exec();
}
