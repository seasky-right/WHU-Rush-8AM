#include "MainWindow.h"
#include "EditorWindow.h" 
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QMessageBox>
#include <QtCore/QDebug>
#include <QtCore/QCoreApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    this->setWindowTitle("WHU Morning Rush - 早八冲锋号");
    this->resize(1200, 800);

    model = new GraphModel();
    mapWidget = new MapWidget(this);
    mapWidget->setEditMode(EditMode::None);

    setupUi();

    connect(mapWidget, &MapWidget::nodeClicked, this, &MainWindow::onMapNodeClicked);
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

MainWindow::~MainWindow() { delete model; }

void MainWindow::setupUi()
{
    QWidget* centralWidget = new QWidget(this);
    this->setCentralWidget(centralWidget);

    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);

    // --- 左侧导航栏 ---
    QGroupBox* controlPanel = new QGroupBox("通勤导航");
    controlPanel->setFixedWidth(320);
    QVBoxLayout* panelLayout = new QVBoxLayout(controlPanel);

    // 1. 地点
    panelLayout->addWidget(new QLabel("起点 (左键点击):"));
    startEdit = new QLineEdit(); startEdit->setReadOnly(true);
    panelLayout->addWidget(startEdit);

    panelLayout->addWidget(new QLabel("终点 (右键点击):"));
    endEdit = new QLineEdit(); endEdit->setReadOnly(true);
    panelLayout->addWidget(endEdit);
    panelLayout->addSpacing(15);
    
    // 2. 环境
    QGroupBox* envBox = new QGroupBox("环境设定");
    QVBoxLayout* envLayout = new QVBoxLayout(envBox);

    envLayout->addWidget(new QLabel("今日天气:"));
    weatherCombo = new QComboBox();
    weatherCombo->addItems({"☀️ 晴朗 (Sunny)", "🌧️ 下雨 (Rainy)", "❄️ 大雪 (Snowy)"});
    envLayout->addWidget(weatherCombo);

    envLayout->addWidget(new QLabel("当前时间:"));
    timeCurrentEdit = new QTimeEdit(QTime::currentTime());
    timeCurrentEdit->setDisplayFormat("HH:mm");
    envLayout->addWidget(timeCurrentEdit);

    envLayout->addWidget(new QLabel("上课时间:"));
    timeClassEdit = new QTimeEdit(QTime(8, 0)); 
    timeClassEdit->setDisplayFormat("HH:mm");
    envLayout->addWidget(timeClassEdit);
    panelLayout->addWidget(envBox);
    panelLayout->addSpacing(15);

    // 3. [修改] 交通工具按钮组
    QLabel* modeLabel = new QLabel("选择出行方式:");
    modeLabel->setStyleSheet("font-weight: bold; margin-bottom: 5px;");
    panelLayout->addWidget(modeLabel);

    // 使用 Grid 布局或 HBox 布局来排列按钮
    QGridLayout* btnLayout = new QGridLayout();
    
    btnWalk = new QPushButton("🚶 步行");
    btnBike = new QPushButton("🚲 单车");
    btnEBike = new QPushButton("🛵 电驴");
    btnRun = new QPushButton("🏃 跑步");
    btnBus = new QPushButton("🚌 校车");

    // 简单的样式设置
    QString btnStyle = "QPushButton { padding: 8px; font-weight: bold; border-radius: 4px; background-color: #F0F2F5; border: 1px solid #DCDFE6; } QPushButton:hover { background-color: #E6E8EB; }";
    btnWalk->setStyleSheet(btnStyle);
    btnBike->setStyleSheet(btnStyle);
    btnEBike->setStyleSheet(btnStyle);
    btnRun->setStyleSheet(btnStyle);
    btnBus->setStyleSheet(btnStyle);

    // 排列：第一行3个，第二行2个
    btnLayout->addWidget(btnWalk, 0, 0);
    btnLayout->addWidget(btnBike, 0, 1);
    btnLayout->addWidget(btnEBike, 0, 2);
    btnLayout->addWidget(btnRun, 1, 0);
    btnLayout->addWidget(btnBus, 1, 1);

    panelLayout->addLayout(btnLayout);

    // 信号连接
    connect(btnWalk, &QPushButton::clicked, this, [this](){ onModeSearch(TransportMode::Walk); });
    connect(btnBike, &QPushButton::clicked, this, [this](){ onModeSearch(TransportMode::SharedBike); });
    connect(btnEBike, &QPushButton::clicked, this, [this](){ onModeSearch(TransportMode::EBike); });
    connect(btnRun, &QPushButton::clicked, this, [this](){ onModeSearch(TransportMode::Run); });
    connect(btnBus, &QPushButton::clicked, this, [this](){ onModeSearch(TransportMode::Bus); });

    panelLayout->addSpacing(20);
    
    // 4. 结果列表
    QLabel* routeLabel = new QLabel("规划结果:");
    routeLabel->setStyleSheet("font-weight: bold;");
    panelLayout->addWidget(routeLabel);
    
    routeScrollArea = new QScrollArea();
    routeScrollArea->setWidgetResizable(true);
    routePanelWidget = new QWidget();
    routePanelLayout = new QVBoxLayout(routePanelWidget);
    routePanelLayout->setAlignment(Qt::AlignTop);
    routeScrollArea->setWidget(routePanelWidget);
    panelLayout->addWidget(routeScrollArea, 1);

    // 5. 底部
    panelLayout->addSpacing(10);
    openEditorBtn = new QPushButton("🛠️ 打开编辑器");
    panelLayout->addWidget(openEditorBtn);

    statusLabel = new QLabel("就绪");
    statusLabel->setStyleSheet("color: gray;");
    panelLayout->addWidget(statusLabel);

    mainLayout->addWidget(controlPanel);
    mainLayout->addWidget(mapWidget, 1);
}

void MainWindow::onMapNodeClicked(int nodeId, QString name, bool isLeftClick) {
    if (isLeftClick) {
        startEdit->setText(name); currentStartId = nodeId; statusLabel->setText("起点: " + name);
    } else {
        endEdit->setText(name); currentEndId = nodeId; statusLabel->setText("终点: " + name);
    }
}

// [新增] 核心搜索逻辑
void MainWindow::onModeSearch(TransportMode mode) {
    if (currentStartId == -1 || currentEndId == -1) {
        QMessageBox::warning(this, "提示", "请先在地图上选择起点(左键)和终点(右键)！");
        return;
    }

    Weather w = Weather::Sunny;
    int idx = weatherCombo->currentIndex();
    if (idx == 1) w = Weather::Rainy;
    if (idx == 2) w = Weather::Snowy;

    QTime curTime = timeCurrentEdit->time();
    QTime clsTime = timeClassEdit->time();

    statusLabel->setText("规划中...");
    
    // 获取单条推荐
    PathRecommendation rec = model->getSpecificRoute(
        currentStartId, currentEndId, mode, w, curTime, clsTime
    );

    QVector<PathRecommendation> results;
    // 只有路径非空才展示
    if (!rec.pathNodeIds.isEmpty()) {
        results.append(rec);
        statusLabel->setText("规划成功");
    } else {
        statusLabel->setText("该方式无可行路线 (可能受天气或地形限制)");
        QMessageBox::information(this, "提示", "该交通方式下无可行路线。\n原因可能是：\n1. 雪天禁行\n2. 楼梯阻断了车辆\n3. 孤立节点");
    }
    
    // 复用之前的展示逻辑
    displayRouteRecommendations(results);
}

void MainWindow::displayRouteRecommendations(const QVector<PathRecommendation>& recommendations) {
    clearRoutePanel();
    currentRecommendations = recommendations;
    for (int i = 0; i < recommendations.size(); ++i) {
        RouteButton* btn = new RouteButton(recommendations[i]);
        routePanelLayout->addWidget(btn);
        connect(btn, &QPushButton::clicked, this, [this, i]() { onRouteButtonClicked(i); });
        connect(btn, &RouteButton::routeHovered, this, &MainWindow::onRouteHovered);
        connect(btn, &RouteButton::routeUnhovered, this, &MainWindow::onRouteUnhovered);
    }
    // 如果有结果，默认高亮第一条
    if (!recommendations.isEmpty()) {
        mapWidget->highlightPath(recommendations[0].pathNodeIds, 1.0);
    } else {
        mapWidget->clearPathHighlight();
    }
}

void MainWindow::clearRoutePanel() {
    QLayoutItem* item;
    while ((item = routePanelLayout->takeAt(0)) != nullptr) {
        delete item->widget(); delete item;
    }
    routeButtons.clear();
}

void MainWindow::onRouteButtonClicked(int routeIndex) {
    if (routeIndex >= 0 && routeIndex < currentRecommendations.size()) {
        const auto& rec = currentRecommendations[routeIndex];
        mapWidget->highlightPath(rec.pathNodeIds, 1.0);
        statusLabel->setText("已选择: " + rec.typeName);
    }
}
void MainWindow::onRouteHovered(const PathRecommendation& recommendation) {
    mapWidget->highlightPath(recommendation.pathNodeIds, 0.8);
}
void MainWindow::onRouteUnhovered() { 
    // 不再清除，保留点击选中的状态，或者根据需求清除
    // mapWidget->clearPathHighlight(); 
}

void MainWindow::onOpenEditor() {
    EditorWindow* editor = new EditorWindow(this->model, this);
    connect(editor, &EditorWindow::dataChanged, this, &MainWindow::onMapDataChanged);
    editor->setWindowModality(Qt::WindowModal); 
    editor->show();
}

void MainWindow::onMapDataChanged() {
    mapWidget->drawMap(model->getAllNodes(), model->getAllEdges());
    statusLabel->setText("地图数据已更新");
}