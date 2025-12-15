#include "MainWindow.h"
// Module-qualified Qt includes
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QMessageBox>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QDir>
#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QLayoutItem>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QComboBox>
#include "../model/MapEditor.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 1. 基础设置
    this->setWindowTitle("WHU Morning Rush - 早八冲锋号");
    this->resize(1400, 800);

    // 2. 初始化核心逻辑
    model = new GraphModel();
    mapWidget = new MapWidget(this);

    // 3. 构建界面布局
    setupUi();

    // 4. 连接信号与槽
    // 当地图被点击 -> 触发 onMapNodeClicked
    connect(mapWidget, &MapWidget::nodeClicked, this, &MainWindow::onMapNodeClicked);
    connect(mapWidget, &MapWidget::editPointPicked, this, &MainWindow::onEditPointPicked);

    // 当按钮被点击 -> 触发 onStartSearch
    connect(searchBtn, &QPushButton::clicked, this, &MainWindow::onStartSearch);

    // 5. 延时加载数据
    QTimer::singleShot(0, this, [this](){
        bool success = false;

        // 尝试资源路径 -> 可执行文件相对的 ./Data -> 应用程序目录下的 Data
        QString appDir = QCoreApplication::applicationDirPath();
        QStringList tryPairs = {
            ":/nodes.txt|:/edges.txt",
            "./Data/nodes.txt|./Data/edges.txt",
            appDir + "/Data/nodes.txt|" + appDir + "/Data/edges.txt"
        };

        for (const QString &pair : tryPairs) {
            QStringList parts = pair.split('|');
            if (parts.size() != 2) continue;
            if (model->loadData(parts[0], parts[1])) {
                success = true;
                statusLabel->setText("数据加载成功： " + parts[0]);
                break;
            }
        }

        if (success) {
            mapWidget->drawMap(model->getAllNodes(), model->getAllEdges());

            // 尝试加载背景图片（优先项目 ./Data，然后应用程序目录）
            QString bg1 = "./Data/map.png";
            QString bg2 = appDir + "/Data/map.png";
            QString bgRes = ":/map.png";

            if (QFileInfo::exists(bg1)) mapWidget->setBackgroundImage(bg1);
            else if (QFileInfo::exists(bg2)) mapWidget->setBackgroundImage(bg2);
            else if (QFileInfo::exists(bgRes)) mapWidget->setBackgroundImage(bgRes);
        } else {
            statusLabel->setText("❌ 数据加载失败！");
            QMessageBox::critical(this, "严重错误",
                                  "找不到数据文件！\n\n"
                                  "请确认 'Data' 文件夹是否位于可执行文件所在目录或工程根目录：\n" + QDir::currentPath());
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
    controlPanel->setFixedWidth(350);
    QVBoxLayout* panelLayout = new QVBoxLayout(controlPanel);

    // 起点
    panelLayout->addWidget(new QLabel("起点 (左键点击地图):"));
    startEdit = new QLineEdit();
    startEdit->setPlaceholderText("请选择起点...");
    startEdit->setReadOnly(true);
    panelLayout->addWidget(startEdit);

    // 终点
    panelLayout->addWidget(new QLabel("终点 (右键点击地图):"));
    endEdit = new QLineEdit();
    endEdit->setPlaceholderText("请选择终点...");
    endEdit->setReadOnly(true);
    panelLayout->addWidget(endEdit);

    // 按钮
    panelLayout->addSpacing(20);
    searchBtn = new QPushButton("🚀 开始推荐");
    searchBtn->setStyleSheet("background-color: #2ECC71; color: white; font-weight: bold; padding: 10px; border-radius: 5px;");
    panelLayout->addWidget(searchBtn);

    // 编辑模式开关
    editModeCheck = new QCheckBox("🛠️ 地图编辑模式 (Ctrl=建筑, 左键；右键=抬笔)");
    panelLayout->addWidget(editModeCheck);
    connect(editModeCheck, &QCheckBox::toggled, this, [this](bool on){
        mapWidget->setEditMode(on);
        statusLabel->setText(on ? "编辑模式已开启：左键添加，右键抬笔" : "编辑模式已关闭");
    });

    // 编辑器表单
    setupEditorPanel(panelLayout);

    // 分隔线
    panelLayout->addSpacing(20);
    
    // 路线推荐面板
    QLabel* routeLabel = new QLabel("推荐路线:");
    routeLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    panelLayout->addWidget(routeLabel);
    
    // 创建滚动区域用于显示路线按钮
    routeScrollArea = new QScrollArea();
    routeScrollArea->setWidgetResizable(true);
    routeScrollArea->setStyleSheet("QScrollArea { border: 1px solid #D0D0D0; border-radius: 3px; }");
    routeScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    routePanelWidget = new QWidget();
    routePanelWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    routePanelLayout = new QVBoxLayout(routePanelWidget);
    routePanelLayout->setContentsMargins(5, 5, 5, 5);
    routePanelLayout->setSpacing(8);
    routePanelLayout->setAlignment(Qt::AlignTop);
    routePanelLayout->addStretch();
    
    routeScrollArea->setWidget(routePanelWidget);
    panelLayout->addWidget(routeScrollArea, 1);  // 给予伸缩空间

    // 状态栏
    panelLayout->addSpacing(10);
    statusLabel = new QLabel("就绪");
    statusLabel->setStyleSheet("color: gray; font-size: 10px;");
    statusLabel->setWordWrap(true);
    panelLayout->addWidget(statusLabel);

    // --- 添加到主布局 ---
    mainLayout->addWidget(controlPanel);
    mainLayout->addWidget(mapWidget, 1);  // mapWidget 占据剩余空间
}

void MainWindow::onMapNodeClicked(int nodeId, QString name, bool isLeftClick)
{
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

    statusLabel->setText("正在计算路线推荐...");
    qDebug() << "开始计算多策略推荐路线...";

    // 获取三条不同权重的路线
    QVector<PathRecommendation> recommendations = model->recommendPaths(currentStartId, currentEndId);

    if (recommendations.isEmpty()) {
        statusLabel->setText("❌ 无法找到可行路线！");
        QMessageBox::warning(this, "提示", "起点和终点之间没有可连通的路径！");
        return;
    }

    qDebug() << "找到" << recommendations.size() << "条推荐路线";
    
    // 显示推荐路线
    displayRouteRecommendations(recommendations);
    
    statusLabel->setText(QString("✅ 找到 %1 条推荐路线，请选择！").arg(recommendations.size()));
}

void MainWindow::displayRouteRecommendations(const QVector<PathRecommendation>& recommendations)
{
    clearRoutePanel();

    currentRecommendations = recommendations;
    routeButtons.clear();
    
    // 创建每个路线的按钮
    for (int i = 0; i < recommendations.size(); ++i) {
        RouteButton* btn = new RouteButton(recommendations[i]);
        routeButtons.append(btn);

        // 添加按钮到面板
        routePanelLayout->addWidget(btn);
        
        // 连接按钮的点击事件
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            onRouteButtonClicked(i);
        });
        
        // 连接悬停事件
        connect(btn, &RouteButton::routeHovered, this, &MainWindow::onRouteHovered);
        connect(btn, &RouteButton::routeUnhovered, this, &MainWindow::onRouteUnhovered);
    }
    
    // 重新添加stretch
    routePanelLayout->addStretch();
}

void MainWindow::clearRoutePanel()
{
    // 清空布局中的所有项目（按钮和占位）
    while (routePanelLayout->count() > 0) {
        QLayoutItem* item = routePanelLayout->takeAt(0);
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }
    routeButtons.clear();
    currentRecommendations.clear();
}

void MainWindow::onRouteButtonClicked(int routeIndex)
{
    if (routeIndex < 0 || routeIndex >= currentRecommendations.size()) {
        return;
    }
    
    const PathRecommendation& rec = currentRecommendations[routeIndex];
    qDebug() << "用户选择了路线:" << rec.typeName;
    
    // 直接使用高亮并启动生长动画（原版本行为）
    mapWidget->highlightPath(rec.pathNodeIds, 1.0);
    
    statusLabel->setText(QString("已选择: %1 (%2) | 距离: %3m | 耗时: %4s")
        .arg(rec.routeLabel)
        .arg(rec.typeName)
        .arg(static_cast<int>(rec.distance))
        .arg(static_cast<int>(rec.duration)));
}

void MainWindow::onRouteHovered(const PathRecommendation& recommendation)
{
    // 当鼠标悬停在某条路线上时，显示预览动画
    qDebug() << "悬停在路线:" << recommendation.typeName;
    mapWidget->highlightPath(recommendation.pathNodeIds, 0.8);  // 预览时间稍短
}

void MainWindow::onRouteUnhovered()
{
    // 当鼠标离开时，清除预览
    qDebug() << "鼠标离开路线";
    mapWidget->clearPathHighlight();
}

// ------------------- 编辑模式 -------------------
void MainWindow::setupEditorPanel(QVBoxLayout* panelLayout)
{
    QGroupBox* editorBox = new QGroupBox("地图编辑器");
    QVBoxLayout* v = new QVBoxLayout(editorBox);

    nodeCoordLabel = new QLabel("坐标: -");
    v->addWidget(nodeCoordLabel);

    nodeNameEdit = new QLineEdit();
    nodeNameEdit->setPlaceholderText("名称 (默认为 building_/road_ 自动生成)");
    v->addWidget(nodeNameEdit);

    nodeTypeCombo = new QComboBox();
    nodeTypeCombo->addItem("可见节点", 0);
    nodeTypeCombo->addItem("幽灵节点", 9);
    v->addWidget(nodeTypeCombo);

    nodeDescEdit = new QLineEdit();
    nodeDescEdit->setPlaceholderText("描述，默认 '无'");
    v->addWidget(nodeDescEdit);

    nodeCategoryEdit = new QLineEdit();
    nodeCategoryEdit->setPlaceholderText("分类，默认 'None'");
    v->addWidget(nodeCategoryEdit);

    connectPrevCheck = new QCheckBox("连接到上一个已保存节点");
    v->addWidget(connectPrevCheck);

    edgeDescEdit = new QLineEdit();
    edgeDescEdit->setPlaceholderText("道路描述，默认 '无'");
    v->addWidget(edgeDescEdit);

    saveNodeBtn = new QPushButton("💾 保存节点 / 可选连边");
    v->addWidget(saveNodeBtn);
    connect(saveNodeBtn, &QPushButton::clicked, this, &MainWindow::onSaveNode);

    panelLayout->addWidget(editorBox);
}

void MainWindow::onEditPointPicked(QPointF pos, bool ctrlPressed)
{
    nodeCoordLabel->setText(QString("坐标: (%1, %2)").arg(pos.x(), 0, 'f', 2).arg(pos.y(), 0, 'f', 2));
    nodeTypeCombo->setCurrentIndex(ctrlPressed ? 0 : 1); // ctrl->建筑
    // 记录坐标在控件属性中
    nodeCoordLabel->setProperty("sceneX", pos.x());
    nodeCoordLabel->setProperty("sceneY", pos.y());
}

void MainWindow::onSaveNode()
{
    if (!mapWidget || !mapWidget->getEditor()) return;

    double x = nodeCoordLabel->property("sceneX").toDouble();
    double y = nodeCoordLabel->property("sceneY").toDouble();
    if (!nodeCoordLabel->property("sceneX").isValid()) {
        QMessageBox::warning(this, "提示", "请先在地图上点击一个点以确定坐标");
        return;
    }

    QString name = nodeNameEdit->text();
    int type = nodeTypeCombo->currentData().toInt();
    QString desc = nodeDescEdit->text();
    QString category = nodeCategoryEdit->text();
    bool connectFlag = connectPrevCheck->isChecked() && (lastSavedNodeId != -1);
    QString edgeDesc = edgeDescEdit->text();

    int newId = mapWidget->getEditor()->createNode(name, QPointF(x,y), type,
            desc, category, lastSavedNodeId, connectFlag, QStringLiteral("自动道路"), edgeDesc);

    // 立即显示在地图上（包括幽灵节点）
    mapWidget->addEditVisualNode(newId, name.isEmpty() ? QString::number(newId) : name, QPointF(x,y), type);

    if (connectFlag) {
        statusLabel->setText(QString("已保存节点 %1 并连接 %2").arg(newId).arg(lastSavedNodeId));
    } else {
        statusLabel->setText(QString("已保存节点 %1").arg(newId));
    }

    lastSavedNodeId = newId;
    nodeNameEdit->clear();
    nodeDescEdit->clear();
    nodeCategoryEdit->clear();
    edgeDescEdit->clear();
}