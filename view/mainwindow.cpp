#include "MainWindow.h"
#include "EditorWindow.h" 
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QMessageBox>
#include <QtCore/QDebug>
#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtWidgets/QGraphicsDropShadowEffect>
#include <QtWidgets/QScrollBar> 
#include <QtWidgets/QSplitter>
#include <QtWidgets/QListWidget>

// ==========================================================================
//  【辅助类】自动补零的 SpinBox (比如显示 08 而不是 8)
// ==========================================================================
class PadSpinBox : public QSpinBox {
public:
    using QSpinBox::QSpinBox; 
protected:
    // 重写显示逻辑：不足2位自动补0
    QString textFromValue(int val) const override {
        return QString("%1").arg(val, 2, 10, QChar('0'));
    }
};

// ==========================================================================
//  【资源】内嵌 SVG 图标 (已改为纯黑色 stroke='%23000000')
// ==========================================================================

// 下箭头 (纯黑)
const QString ICON_CHEVRON_DOWN = 
    "url(\"data:image/svg+xml;charset=utf-8,"
    "<svg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 24 24' fill='none' stroke='%23000000' stroke-width='3' stroke-linecap='round' stroke-linejoin='round'>"
    "<polyline points='6 9 12 15 18 9'></polyline>"
    "</svg>\")";

// 上箭头 (纯黑)
const QString ICON_CHEVRON_UP = 
    "url(\"data:image/svg+xml;charset=utf-8,"
    "<svg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 24 24' fill='none' stroke='%23000000' stroke-width='3' stroke-linecap='round' stroke-linejoin='round'>"
    "<polyline points='18 15 12 9 6 15'></polyline>"
    "</svg>\")";

// --------------------------------------------------------------------------
//  全局滚动条样式
// --------------------------------------------------------------------------
const QString SCROLL_STYLE = 
    "QScrollBar:vertical { background: transparent; width: 8px; margin: 0px; }"
    "QScrollBar::handle:vertical { background: #C1C1C5; min-height: 20px; border-radius: 4px; }"
    "QScrollBar::handle:vertical:hover { background: #8E8E93; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
    "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    "QScrollBar:horizontal { background: transparent; height: 8px; margin: 0px; }"
    "QScrollBar::handle:horizontal { background: #C1C1C5; min-width: 20px; border-radius: 4px; }"
    "QScrollBar::handle:horizontal:hover { background: #8E8E93; }"
    "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
    "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    this->setWindowTitle("WHU Morning Rush - 早八冲锋号");
    this->resize(1280, 850);

    model = new GraphModel();
    mapWidget = new MapWidget(this);
    mapWidget->setEditMode(EditMode::None);

    setupUi();

    connect(mapWidget, &MapWidget::nodeClicked, this, &MainWindow::onMapNodeClicked);
    connect(openEditorBtn, &QPushButton::clicked, this, &MainWindow::onOpenEditor);

    QString appDir = QCoreApplication::applicationDirPath();
    
    // 【修改】同时加载地图和时刻表
    bool mapLoaded = model->loadData(appDir + "/Data/nodes.txt", appDir + "/Data/edges.txt");
    bool scheduleLoaded = model->loadSchedule(appDir + "/Data/bus_schedule.csv");

    if (mapLoaded) {
        mapWidget->drawMap(model->getAllNodes(), model->getAllEdges());
        mapWidget->setBackgroundImage(appDir + "/Data/map.png");
        
        if (scheduleLoaded) {
            statusLabel->setText("地图与时刻表加载成功");
        } else {
            statusLabel->setText("注意：校车时刻表加载失败");
        }
    } else {
        statusLabel->setText("数据加载失败");
    }
}

MainWindow::~MainWindow() {
    delete model;
}

// 辅助函数：创建美化的时间选择器 (时 : 分)
QWidget* createTimeSpinner(QSpinBox*& spinHour, QSpinBox*& spinMin, QTime defaultTime) {
    QWidget* container = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto setupSpin = [](QSpinBox* spin, int max) {
        spin->setRange(0, max);
        spin->setAlignment(Qt::AlignCenter);
        spin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
        
        spin->setStyleSheet(
            "QSpinBox { "
            "    background-color: #FFFFFF; "
            "    border: 1px solid #D1D1D6; "
            "    border-radius: 8px; "
            "    padding: 8px 4px; "
            "    font-size: 16px; color: #1C1C1E; font-weight: bold;"
            "}"
            "QSpinBox:focus { border: 2px solid #007AFF; }"
            
            // 按钮区域
            "QSpinBox::up-button, QSpinBox::down-button { "
            "    width: 24px; " 
            "    background: transparent; "
            "    border: none; "
            "    border-left: 1px solid #F2F2F7; " 
            "}"
            "QSpinBox::up-button:hover, QSpinBox::down-button:hover { "
            "    background-color: #E5E5EA; "
            "}"
            
            // 使用黑色 SVG 图标
            "QSpinBox::up-arrow { "
            "    image: " + ICON_CHEVRON_UP + "; "
            "    width: 10px; height: 10px; "
            "}"
            "QSpinBox::down-arrow { "
            "    image: " + ICON_CHEVRON_DOWN + "; "
            "    width: 10px; height: 10px; "
            "}"
        );
    };

    // 使用 PadSpinBox 替代 QSpinBox 以支持自动补零
    spinHour = new PadSpinBox();
    setupSpin(spinHour, 23);
    spinHour->setValue(defaultTime.hour());
    
    spinMin = new PadSpinBox();
    setupSpin(spinMin, 59);
    spinMin->setValue(defaultTime.minute());

    QLabel* sep = new QLabel(":");
    sep->setStyleSheet("font-size: 20px; font-weight: bold; color: #C7C7CC; margin-bottom: 2px;");

    layout->addWidget(spinHour, 1);
    layout->addWidget(sep);
    layout->addWidget(spinMin, 1);
    
    return container;
}

void MainWindow::setupUi() {
    QWidget* centralWidget = new QWidget(this);
    this->setCentralWidget(centralWidget);
    centralWidget->setStyleSheet("background-color: #F2F2F7;"); 

    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 使用 Splitter 分割左右 (左边控制，右边地图)
    QSplitter* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);

    // === 左侧控制面板 ===
    QWidget* leftWidget = new QWidget();
    leftWidget->setMinimumWidth(320);
    leftWidget->setMaximumWidth(380);
    leftWidget->setStyleSheet("background-color: #FFFFFF; border-right: 1px solid #D1D1D6;");
    
    QVBoxLayout* panelLayout = new QVBoxLayout(leftWidget);
    panelLayout->setContentsMargins(15, 20, 15, 20);
    panelLayout->setSpacing(10);

    // 标题
    QLabel* titleLabel = new QLabel("WHU Rush 🚀");
    titleLabel->setStyleSheet("font-size: 22px; font-weight: 900; color: #1C1C1E; margin-bottom: 10px;");
    panelLayout->addWidget(titleLabel);

    // 1. 路线设定组
    QGroupBox* grpRoute = new QGroupBox("路线设定");
    grpRoute->setStyleSheet("QGroupBox { font-weight: bold; color: #8E8E93; border: 1px solid #E5E5EA; border-radius: 8px; margin-top: 5px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; }");
    QVBoxLayout* routeLayout = new QVBoxLayout(grpRoute);
    
    startEdit = new QLineEdit(); startEdit->setPlaceholderText("🟢 起点 (左键)"); startEdit->setReadOnly(true);
    endEdit = new QLineEdit(); endEdit->setPlaceholderText("🔴 终点 (右键)"); endEdit->setReadOnly(true);
    routeLayout->addWidget(startEdit);
    routeLayout->addWidget(endEdit);

    // [Task 3] 途经点控件
    QHBoxLayout* wpLayout = new QHBoxLayout();
    waypointCheck = new QCheckBox("添加途经点模式");
    QPushButton* btnClearWp = new QPushButton("清空");
    btnClearWp->setFixedWidth(50);
    connect(btnClearWp, &QPushButton::clicked, this, [this](){ 
        currentWaypoints.clear(); 
        waypointList->clear(); 
        statusLabel->setText("途经点已清空");
    });
    wpLayout->addWidget(waypointCheck);
    wpLayout->addWidget(btnClearWp);
    routeLayout->addLayout(wpLayout);

    waypointList = new QListWidget();
    waypointList->setFixedHeight(50);
    waypointList->setStyleSheet("border: 1px solid #E5E5EA; border-radius: 4px; font-size: 11px; background: #FAFAFA;");
    routeLayout->addWidget(waypointList);
    panelLayout->addWidget(grpRoute);

    // 2. 环境参数组
    QGroupBox* grpEnv = new QGroupBox("环境参数");
    grpEnv->setStyleSheet(grpRoute->styleSheet());
    QGridLayout* envLayout = new QGridLayout(grpEnv);
    
    weatherCombo = new QComboBox();
    weatherCombo->addItems({"☀️ 晴朗", "🌧️ 下雨", "❄️ 大雪"});
    envLayout->addWidget(new QLabel("天气:"), 0, 0);
    envLayout->addWidget(weatherCombo, 0, 1);

    QWidget* currTimeW = createTimeSpinner(spinCurrHour, spinCurrMin, QTime::currentTime());
    envLayout->addWidget(new QLabel("出发:"), 1, 0);
    envLayout->addWidget(currTimeW, 1, 1);

    QWidget* classTimeW = createTimeSpinner(spinClassHour, spinClassMin, QTime(8, 0));
    envLayout->addWidget(new QLabel("早八:"), 2, 0);
    envLayout->addWidget(classTimeW, 2, 1);
    
    lateCheckToggle = new QCheckBox("迟到预警");
    lateCheckToggle->setChecked(true);
    envLayout->addWidget(lateCheckToggle, 3, 1, Qt::AlignRight);
    panelLayout->addWidget(grpEnv);

    // 3. 出行方式
    QGridLayout* modeLayout = new QGridLayout();
    btnWalk = new QPushButton("🚶"); btnWalk->setToolTip("步行");
    btnBike = new QPushButton("🚲"); btnBike->setToolTip("共享单车");
    btnEBike = new QPushButton("🛵"); btnEBike->setToolTip("电动车");
    btnRun = new QPushButton("🏃"); btnRun->setToolTip("跑步");
    btnBus = new QPushButton("🚌"); btnBus->setToolTip("校车");
    
    QVector<QPushButton*> modes = {btnWalk, btnBike, btnEBike, btnRun, btnBus};
    int col = 0;
    for(auto* btn : modes) {
        btn->setFixedSize(50, 40);
        btn->setCursor(Qt::PointingHandCursor);
        // 连接点击事件
        connect(btn, &QPushButton::clicked, this, [=](){
            if(btn == btnWalk) onModeSearch(TransportMode::Walk);
            else if(btn == btnBike) onModeSearch(TransportMode::SharedBike);
            else if(btn == btnEBike) onModeSearch(TransportMode::EBike);
            else if(btn == btnRun) onModeSearch(TransportMode::Run);
            else if(btn == btnBus) onModeSearch(TransportMode::Bus);
        });
        modeLayout->addWidget(btn, 0, col++);
    }
    panelLayout->addLayout(modeLayout);

    // 4. 结果区
    routeScrollArea = new QScrollArea();
    routeScrollArea->setWidgetResizable(true);
    routeScrollArea->setStyleSheet("background: transparent; border: none;");
    routePanelWidget = new QWidget();
    routePanelLayout = new QVBoxLayout(routePanelWidget);
    routePanelLayout->setAlignment(Qt::AlignTop);
    routePanelLayout->setSpacing(8);
    routeScrollArea->setWidget(routePanelWidget);
    panelLayout->addWidget(routeScrollArea, 1);

    // 5. 底部
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    openEditorBtn = new QPushButton("🛠️ 编辑地图");
    statusLabel = new QLabel("Ready");
    statusLabel->setStyleSheet("color: #8E8E93; font-size: 11px;");
    bottomLayout->addWidget(openEditorBtn);
    bottomLayout->addWidget(statusLabel);
    panelLayout->addLayout(bottomLayout);

    // 组装 Splitter
    splitter->addWidget(leftWidget);
    splitter->addWidget(mapWidget);
    splitter->setStretchFactor(1, 1); // 地图占大头
    
    mainLayout->addWidget(splitter);
}

void MainWindow::resetAllButtonStyles() {
    updateButtonStyle(btnWalk, false, false);
    updateButtonStyle(btnBike, false, false);
    updateButtonStyle(btnEBike, false, false);
    updateButtonStyle(btnRun, false, false);
    updateButtonStyle(btnBus, false, false);
}

void MainWindow::updateButtonStyle(QPushButton* btn, bool isSelected, bool isLate) {
    if (!btn) return;

    QString style;
    QString base = "border-radius: 10px; padding: 12px 0px; font-weight: bold; font-size: 13px; ";

    if (!isSelected) {
        // 未选中：浅灰背景
        style = "QPushButton { "
                "    background-color: #F7F7F9; "
                "    color: #3A3A3C; "
                "    border: 1px solid #E5E5EA; " + base +
                "} "
                "QPushButton:hover { background-color: #FFFFFF; border-color: #C7C7CC; }";
    } else {
        if (isLate) {
            // 迟到：浅红背景
            style = "QPushButton { "
                    "    background-color: #FFEBEE; " 
                    "    color: #C62828; "            
                    "    border: 1px solid #FFCDD2; " + base +
                    "}";
        } else {
            // 准时：浅绿背景
            style = "QPushButton { "
                    "    background-color: #E8F5E9; " 
                    "    color: #2E7D32; "            
                    "    border: 1px solid #C8E6C9; " + base +
                    "}";
        }
    }
    btn->setStyleSheet(style);
    btn->setCursor(Qt::PointingHandCursor);
}

void MainWindow::onModeSearch(TransportMode mode) {
    if (currentStartId == -1 || currentEndId == -1) {
        QMessageBox::warning(this, "提示", "请先在地图上选择起点和终点！");
        return;
    }

    Weather w = Weather::Sunny;
    int idx = weatherCombo->currentIndex();
    if (idx == 1) w = Weather::Rainy;
    if (idx == 2) w = Weather::Snowy;
    mapWidget->setWeather(w);

    QTime curTime(spinCurrHour->value(), spinCurrMin->value());
    QTime clsTime(spinClassHour->value(), spinClassMin->value());
    bool checkLate = lateCheckToggle->isChecked();

    statusLabel->setText("正在规划多策略路线...");
    resetAllButtonStyles();

    // [关键修改] 调用新的多策略接口，传入 currentWaypoints
    QVector<PathRecommendation> results = model->getMultiStrategyRoutes(
        currentStartId, currentEndId, currentWaypoints, 
        mode, w, curTime, clsTime, checkLate
    );

    // UI 反馈
    QPushButton* currentBtn = nullptr;
    if (mode == TransportMode::Walk) currentBtn = btnWalk;
    else if (mode == TransportMode::SharedBike) currentBtn = btnBike;
    else if (mode == TransportMode::EBike) currentBtn = btnEBike;
    else if (mode == TransportMode::Run) currentBtn = btnRun;
    else if (mode == TransportMode::Bus) currentBtn = btnBus;

    if (currentBtn) {
        bool anyLate = false;
        // 如果所有推荐路线都迟到，按钮才变红
        if (!results.isEmpty()) {
            anyLate = true; 
            for(auto& r : results) if(!r.isLate) anyLate = false;
        }
        updateButtonStyle(currentBtn, true, anyLate);
    }

    if (results.isEmpty()) {
        statusLabel->setText("无可行路线");
        QMessageBox::information(this, "提示", "无法找到路径。\n请检查是否被雪天/楼梯阻断，或节点不连通。");
    } else {
        statusLabel->setText(QString("规划完成，找到 %1 种方案").arg(results.size()));
    }
    
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

/**
 * @brief 处理地图节点点击事件
 * * 当用户在地图上点击某个节点（建筑物或路口）时，MapWidget 会发送此信号。
 * 根据鼠标按键的不同，该函数将选中的节点设置为“起点”或“终点”。
 * * @param nodeId        被点击节点的唯一 ID
 * @param name          被点击节点的名称（用于显示）
 * @param isLeftClick   如果是左键点击则为 true（设为起点），右键为 false（设为终点）
 */
void MainWindow::onMapNodeClicked(int nodeId, QString name, bool isLeftClick)
{
    // 打印调试日志，方便追踪交互流程
    qDebug() << "[Interaction] User clicked a node on map.";
    qDebug() << "    - Node ID:" << nodeId;
    qDebug() << "    - Node Name:" << name;
    qDebug() << "    - Mouse Button:" << (isLeftClick ? "Left (Start)" : "Right (End)");

    // 判断是左键还是右键，从而决定是设置起点还是终点
    if (isLeftClick) 
    {
        // === 设置起点逻辑 ===
        
        // 1. 更新内部 ID 变量
        this->currentStartId = nodeId;

        // 2. 更新 UI 文本框显示
        // 我们加上一个绿色的圆点符号，增强视觉反馈
        if (this->startEdit) 
        {
            this->startEdit->setText(QString("🟢 %1").arg(name));
        }

        // 3. 更新底部状态栏，给用户即时反馈
        if (this->statusLabel) 
        {
            this->statusLabel->setText(QString("已选择起点: %1").arg(name));
        }
    } 
    else 
    {
        // === 设置终点逻辑 ===

        // 1. 更新内部 ID 变量
        this->currentEndId = nodeId;

        // 2. 更新 UI 文本框显示
        // 我们加上一个红色的圆点符号，代表目标
        if (this->endEdit) 
        {
            this->endEdit->setText(QString("🔴 %1").arg(name));
        }

        // 3. 更新底部状态栏
        if (this->statusLabel) 
        {
            this->statusLabel->setText(QString("已选择终点: %1").arg(name));
        }
    }
    
    // 如果起点和终点都已就绪，可以在这里重置之前的路径显示（可选）
    // mapWidget->clearPathHighlight();
}