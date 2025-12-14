#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include "GraphModel.h"
#include "MapWidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 槽函数：处理地图点击
    void onMapNodeClicked(int nodeId, QString name, bool isLeftClick);

    // 槽函数：点击“开始监测”按钮
    void onStartSearch();

private:
    GraphModel* model;
    MapWidget* mapWidget;

    // UI 控件
    QLineEdit* startEdit;
    QLineEdit* endEdit;
    QPushButton* searchBtn;
    QLabel* statusLabel;

    // 记录当前选中的 ID
    int currentStartId = -1;
    int currentEndId = -1;

    void setupUi(); // 初始化界面的辅助函数
};
#endif // MAINWINDOW_H
