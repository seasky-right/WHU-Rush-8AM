#include "view/MainWindow.h"
#include <QtWidgets/QApplication>

/**
 * @brief 应用程序入口点
 * 
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return int 应用程序退出代码
 */
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 创建并显示主窗口
    // MainWindow 的构造函数里会自动加载数据并画图
    MainWindow w;
    w.show();

    return a.exec();
}
