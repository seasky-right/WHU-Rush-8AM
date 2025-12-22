// ============================================================
// main.cpp - 程序的入口文件
// 这个文件是整个程序的起点，程序从这里开始运行
// ============================================================

#include "view/MainWindow.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    // 1.创建一个 Qt 应用程序对象
    // 命令行参数:argc 和 argv 
    QApplication app(argc, argv);

    // 2.创建主窗口对象
    MainWindow mainWindow;

    // 3.把主窗口显示出来
    mainWindow.show();

    // 4.让程序进入"事件循环"
    // exec() 会让程序一直运行，等待用户的操作（比如点击按钮、移动鼠标）
    // 直到用户关闭窗口，exec() 才会返回，程序才会结束
    int exitCode = app.exec();

    // 5.返回退出码
    // 退出码为 0 表示程序正常结束，其他值表示出了问题
    return exitCode;
}
