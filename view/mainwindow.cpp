#include "MainWindow.h"
#include "EditorWindow.h" // 引入编辑器窗口
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QMessageBox>
#include <QtCore/QDebug>
#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    this->setWindowTitle("WHU Morning Rush - 早八冲锋号");
    this->resize(1200, 800);

    model = new GraphModel();
    mapWidget = new MapWidget(this);
    // 确保主界面处于 View 模式
    mapWidget->setEditMode(EditMode::None);

    setupUi();

    // 导航信号连接 (注意这里连接的是 nodeClicked)
    connect(mapWidget, &MapWidget::nodeClicked, this, &MainWindow::onMapNodeClicked);
    connect(searchBtn, &QPushButton::clicked, this, &MainWindow::onStartSearch);
    connect(openEditorBtn, &QPushButton::clicked, this, &MainWindow::onOpenEditor);

    // 加载数据
    QString appDir = QCoreApplication::applicationDirPath();
    if (model->loadData(appDir + "/Data/nodes.txt", appDir + "/Data/edges.txt")) {
        mapWidget->drawMap(model->getAllNodes(), model->getAllEdges());
        mapWidget->setBackgroundImage(appDir + "/Data/map.png");
        statusLabel->setText("地图加载成功");
    } else {
        statusLabel->setText("数据加载失败");
    }
}

MainWindow::~MainWindow() {
    delete model;
}

void MainWindow::setupUi()
{
    QWidget* centralWidget = new QWidget(this);
    this->setCentralWidget(centralWidget);

    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);

    // --- 左侧导航栏 ---
    QGroupBox* controlPanel = new QGroupBox("通勤导航");
    controlPanel->setFixedWidth(320);
    QVBoxLayout* panelLayout = new QVBoxLayout(controlPanel);

    panelLayout->addWidget(new QLabel("起点 (左键点击):"));
    startEdit = new QLineEdit(); startEdit->setReadOnly(true);
    panelLayout->addWidget(startEdit);

    panelLayout->addWidget(new QLabel("终点 (右键点击):"));
    endEdit = new QLineEdit(); endEdit->setReadOnly(true);
    panelLayout->addWidget(endEdit);

    panelLayout->addSpacing(10);
    searchBtn = new QPushButton("🚀 开始推荐");
    searchBtn->setStyleSheet("background-color: #2ECC71; color: white; font-weight: bold; padding: 10px; border-radius: 5px;");
    panelLayout->addWidget(searchBtn);

    panelLayout->addSpacing(20);
    
    // 路线展示区
    QLabel* routeLabel = new QLabel("推荐方案:");
    routeLabel->setStyleSheet("font-weight: bold;");
    panelLayout->addWidget(routeLabel);
    
    routeScrollArea = new QScrollArea();
    routeScrollArea->setWidgetResizable(true);
    routePanelWidget = new QWidget();
    routePanelLayout = new QVBoxLayout(routePanelWidget);
    routePanelLayout->setAlignment(Qt::AlignTop);
    routeScrollArea->setWidget(routePanelWidget);
    panelLayout->addWidget(routeScrollArea, 1);

    // 底部工具
    panelLayout->addSpacing(10);
    openEditorBtn = new QPushButton("🛠️ 打开地图编辑器");
    panelLayout->addWidget(openEditorBtn);

    statusLabel = new QLabel("欢迎使用");
    statusLabel->setStyleSheet("color: gray;");
    panelLayout->addWidget(statusLabel);

    mainLayout->addWidget(controlPanel);
    mainLayout->addWidget(mapWidget, 1);
}

void MainWindow::onMapNodeClicked(int nodeId, QString name, bool isLeftClick)
{
    if (isLeftClick) {
        startEdit->setText(name);
        currentStartId = nodeId;
        statusLabel->setText("起点: " + name);
    } else {
        endEdit->setText(name);
        currentEndId = nodeId;
        statusLabel->setText("终点: " + name);
    }
}

void MainWindow::onStartSearch()
{
    if (currentStartId == -1 || currentEndId == -1) {
        QMessageBox::warning(this, "提示", "请先选择起点和终点！");
        return;
    }

    statusLabel->setText("计算中...");
    QVector<PathRecommendation> recommendations = model->recommendPaths(currentStartId, currentEndId);

    if (recommendations.isEmpty()) {
        statusLabel->setText("无可行路线");
        QMessageBox::warning(this, "提示", "无法找到路径！");
        return;
    }

    displayRouteRecommendations(recommendations);
    statusLabel->setText(QString("找到 %1 条路线").arg(recommendations.size()));
}

void MainWindow::displayRouteRecommendations(const QVector<PathRecommendation>& recommendations)
{
    clearRoutePanel();
    currentRecommendations = recommendations;
    
    for (int i = 0; i < recommendations.size(); ++i) {
        RouteButton* btn = new RouteButton(recommendations[i]);
        routePanelLayout->addWidget(btn);
        
        connect(btn, &QPushButton::clicked, this, [this, i]() { onRouteButtonClicked(i); });
        connect(btn, &RouteButton::routeHovered, this, &MainWindow::onRouteHovered);
        connect(btn, &RouteButton::routeUnhovered, this, &MainWindow::onRouteUnhovered);
    }
}

void MainWindow::clearRoutePanel()
{
    QLayoutItem* item;
    while ((item = routePanelLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    routeButtons.clear();
}

void MainWindow::onRouteButtonClicked(int routeIndex)
{
    if (routeIndex >= 0 && routeIndex < currentRecommendations.size()) {
        const auto& rec = currentRecommendations[routeIndex];
        mapWidget->highlightPath(rec.pathNodeIds, 1.0);
        statusLabel->setText("已选择: " + rec.typeName);
    }
}

void MainWindow::onRouteHovered(const PathRecommendation& recommendation)
{
    mapWidget->highlightPath(recommendation.pathNodeIds, 0.8);
}

void MainWindow::onRouteUnhovered()
{
    mapWidget->clearPathHighlight();
}

void MainWindow::onOpenEditor()
{
    // 创建并显示编辑器，传入共享的 Model
    EditorWindow* editor = new EditorWindow(this->model, this);
    // 当编辑器保存数据时，刷新主地图
    connect(editor, &EditorWindow::dataChanged, this, &MainWindow::onMapDataChanged);
    // 设置为独立窗口显示
    editor->setWindowModality(Qt::WindowModal); // 或者 Qt::NonModal
    editor->show();
}

void MainWindow::onMapDataChanged()
{
    // 重新从 Model 绘制地图
    mapWidget->drawMap(model->getAllNodes(), model->getAllEdges());
    statusLabel->setText("地图数据已更新");
}