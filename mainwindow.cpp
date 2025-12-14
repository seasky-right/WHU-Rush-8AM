#include "mainwindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 1. 基础设置
    this->setWindowTitle("WHU Morning Rush - 早八冲锋号");
    this->resize(1200, 800);

    // 2. 初始化核心逻辑
    model = new GraphModel();
    mapWidget = new MapWidget(this);

    // 3. 构建界面布局
    setupUi();

    // 4. 连接信号与槽
    // 当地图被点击 -> 触发 onMapNodeClicked
    connect(mapWidget, &MapWidget::nodeClicked, this, &MainWindow::onMapNodeClicked);

    // 当按钮被点击 -> 触发 onStartSearch
    connect(searchBtn, &QPushButton::clicked, this, &MainWindow::onStartSearch);

    // 5. 延时加载数据
    QTimer::singleShot(0, this, [this](){
        bool success = false;

        if (model->loadData(":/nodes.txt", ":/edges.txt")) {
            success = true;
            statusLabel->setText("资源加载成功。");
        }
        else if(model->loadData("./Data/nodes.txt", "./Data/edges.txt")) {
            success = true;
            statusLabel->setText("本地加载成功。");
        }

        if (success) {
            mapWidget->drawMap(model->getAllNodes(), model->getAllEdges());
        } else {
            statusLabel->setText("❌ 数据加载失败！");
            QMessageBox::critical(this, "严重错误",
                                  "找不到数据文件！\n\n"
                                  "请确认 'Data' 文件夹是否在构建目录下：\n" + QDir::currentPath());
        }
    });
}

MainWindow::~MainWindow()
{
    delete model;
}

void MainWindow::setupUi()
{
    QWidget* centralWidget = new QWidget(this);
    this->setCentralWidget(centralWidget);

    // 总布局：水平 (左边栏 | 右地图)
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);

    // --- 左侧控制栏 ---
    QGroupBox* controlPanel = new QGroupBox("通勤控制台");
    controlPanel->setFixedWidth(300); // 固定宽度
    QVBoxLayout* panelLayout = new QVBoxLayout(controlPanel);

    // 起点
    panelLayout->addWidget(new QLabel("起点 (左键点击地图):"));
    startEdit = new QLineEdit();
    startEdit->setPlaceholderText("请选择起点...");
    startEdit->setReadOnly(true); // 暂时只允许点击选择
    panelLayout->addWidget(startEdit);

    // 终点
    panelLayout->addWidget(new QLabel("终点 (右键点击地图):"));
    endEdit = new QLineEdit();
    endEdit->setPlaceholderText("请选择终点...");
    endEdit->setReadOnly(true);
    panelLayout->addWidget(endEdit);

    // 按钮
    panelLayout->addSpacing(20);
    searchBtn = new QPushButton("🚀 开始监测");
    searchBtn->setStyleSheet("background-color: #2ECC71; color: white; font-weight: bold; padding: 10px; border-radius: 5px;");
    panelLayout->addWidget(searchBtn);

    // 状态栏
    panelLayout->addStretch(); // 弹簧
    statusLabel = new QLabel("就绪");
    statusLabel->setStyleSheet("color: gray;");
    panelLayout->addWidget(statusLabel);

    // --- 添加到主布局 ---
    mainLayout->addWidget(controlPanel);
    mainLayout->addWidget(mapWidget);
}

void MainWindow::onMapNodeClicked(int nodeId, QString name, bool isLeftClick)
{
    qDebug() << "MainWindow received click:" << name << (isLeftClick ? "Left" : "Right"); // 调试输出
    if (isLeftClick) {
        startEdit->setText(name);
        currentStartId = nodeId;
        statusLabel->setText("已设置起点: " + name);
    } else {
        endEdit->setText(name);
        currentEndId = nodeId;
        statusLabel->setText("已设置终点: " + name);
    }
}

void MainWindow::onStartSearch()
{
    if (currentStartId == -1 || currentEndId == -1) {
        QMessageBox::warning(this, "提示", "请先在地图上选择起点和终点！");
        return;
    }

    // 这里将在下一步接入 Dijkstra 算法
    QString msg = QString("准备计算从 ID:%1 到 ID:%2 的路径...").arg(currentStartId).arg(currentEndId);
    statusLabel->setText(msg);
    qDebug() << msg;

    QVector<int> pathIds = model->findPath(currentStartId, currentEndId);

    if (pathIds.isEmpty()) {
        statusLabel->setText("❌ 无法到达！");
        QMessageBox::warning(this, "Oops", "这两个点之间没有路连通！");
        return;
    }

    QString pathStr = "路径: ";
    for (int id : pathIds) {
        Node n = model->getNode(id);
        pathStr += n.name + " -> ";
    }
    pathStr.chop(4);

    qDebug() << "计算成功！" << pathStr;
    statusLabel->setText("✅ 规划成功！请查看控制台输出");
}
