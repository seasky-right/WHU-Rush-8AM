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

    // 【修改 2】使用 PadSpinBox 替代 QSpinBox 以支持自动补零
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

void MainWindow::setupUi()
{
    QWidget* centralWidget = new QWidget(this);
    this->setCentralWidget(centralWidget);
    
    centralWidget->setStyleSheet("background-color: #F2F2F7;"); 

    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(24, 24, 24, 24); 
    mainLayout->setSpacing(24);

    // 1. 左侧控制面板
    QFrame* controlPanel = new QFrame();
    controlPanel->setFixedWidth(360);
    controlPanel->setStyleSheet(
        "QFrame { "
        "    background-color: #FFFFFF; "
        "    border-radius: 18px; "
        "    border: 1px solid #FFFFFF; " 
        "}"
    );
    
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(50);
    shadow->setColor(QColor(0, 0, 0, 40)); 
    shadow->setOffset(0, 12);
    controlPanel->setGraphicsEffect(shadow);

    QVBoxLayout* panelLayout = new QVBoxLayout(controlPanel);
    panelLayout->setContentsMargins(28, 32, 28, 32);
    panelLayout->setSpacing(20);

    QLabel* titleLabel = new QLabel("通勤规划");
    titleLabel->setStyleSheet("font-size: 28px; font-weight: 800; color: #1C1C1E; border: none; background: transparent;");
    panelLayout->addWidget(titleLabel);
    
    panelLayout->addSpacing(8);

    auto setupInput = [](QWidget* w, QString placeholder) {
        w->setStyleSheet(
            "QLineEdit, QComboBox { "
            "    background-color: #FFFFFF; "
            "    border: 1px solid #D1D1D6; " 
            "    border-radius: 8px; "
            "    padding: 10px 12px; "
            "    font-size: 14px; "
            "    color: #1C1C1E; "
            "}"
            "QLineEdit:focus, QComboBox:focus { "
            "    border: 2px solid #007AFF; "
            "}"
            "QComboBox::drop-down { border: none; width: 32px; }"
            "QComboBox::down-arrow { "
            "    image: " + ICON_CHEVRON_DOWN + "; "
            "    width: 12px; height: 12px; "
            "}"
        );
        if (auto* le = qobject_cast<QLineEdit*>(w)) le->setPlaceholderText(placeholder);
    };

    auto createLabel = [](QString text) {
        QLabel* l = new QLabel(text);
        l->setStyleSheet("color: #8E8E93; font-size: 13px; font-weight: 600; border: none; background: transparent; margin-bottom: 4px;");
        return l;
    };

    // 起终点
    panelLayout->addWidget(createLabel("起点 (START)"));
    startEdit = new QLineEdit(); 
    startEdit->setReadOnly(true);
    setupInput(startEdit, "👆 左键点击地图选点");
    panelLayout->addWidget(startEdit);

    panelLayout->addWidget(createLabel("终点 (END)"));
    endEdit = new QLineEdit(); 
    endEdit->setReadOnly(true);
    setupInput(endEdit, "👆 右键点击地图选点");
    panelLayout->addWidget(endEdit);

    // 环境 Grid
    QGridLayout* envGrid = new QGridLayout();
    envGrid->setSpacing(16);

    envGrid->addWidget(createLabel("今日天气"), 0, 0);
    weatherCombo = new QComboBox();
    // 【修改 1】去掉了英文，只保留 emoji 和中文
    weatherCombo->addItems({"☀️ 晴朗", "🌧️ 下雨", "❄️ 大雪"});
    weatherCombo->setCursor(Qt::PointingHandCursor);
    setupInput(weatherCombo, "");
    envGrid->addWidget(weatherCombo, 1, 0);

    envGrid->addWidget(createLabel("当前时间"), 0, 1);
    QWidget* currTimeWidget = createTimeSpinner(spinCurrHour, spinCurrMin, QTime::currentTime());
    envGrid->addWidget(currTimeWidget, 1, 1);

    panelLayout->addLayout(envGrid);

    // 上课时间
    panelLayout->addWidget(createLabel("早八/打卡时间"));
    QHBoxLayout* classTimeLayout = new QHBoxLayout();
    
    QWidget* classTimeWidget = createTimeSpinner(spinClassHour, spinClassMin, QTime(8, 0));
    classTimeLayout->addWidget(classTimeWidget, 1);

    // 复选框
    lateCheckToggle = new QCheckBox("迟到预警");
    lateCheckToggle->setChecked(true);
    lateCheckToggle->setCursor(Qt::PointingHandCursor);
    lateCheckToggle->setStyleSheet(
        "QCheckBox { "
        "    color: #3A3A3C; "
        "    font-size: 14px; "
        "    font-weight: 500; "
        "    border: none; "
        "    background: transparent; "
        "    margin-left: 10px;"
        "}"
    );
    classTimeLayout->addWidget(lateCheckToggle);
    panelLayout->addLayout(classTimeLayout);

    panelLayout->addSpacing(15);

    // 交通工具
    QLabel* modeTitle = new QLabel("选择出行方式");
    modeTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: #1C1C1E; border: none; background: transparent;");
    panelLayout->addWidget(modeTitle);

    QGridLayout* btnLayout = new QGridLayout();
    btnLayout->setSpacing(12);
    
    btnWalk = new QPushButton("🚶 步行");
    btnBike = new QPushButton("🚲 单车");
    btnEBike = new QPushButton("🛵 电驴");
    btnRun = new QPushButton("🏃 跑步");
    btnBus = new QPushButton("🚌 校车");

    resetAllButtonStyles(); // 应用初始样式

    btnLayout->addWidget(btnWalk, 0, 0);
    btnLayout->addWidget(btnBike, 0, 1);
    btnLayout->addWidget(btnEBike, 0, 2);
    btnLayout->addWidget(btnRun, 1, 0);
    btnLayout->addWidget(btnBus, 1, 1);

    panelLayout->addLayout(btnLayout);

    connect(btnWalk, &QPushButton::clicked, this, [this](){ onModeSearch(TransportMode::Walk); });
    connect(btnBike, &QPushButton::clicked, this, [this](){ onModeSearch(TransportMode::SharedBike); });
    connect(btnEBike, &QPushButton::clicked, this, [this](){ onModeSearch(TransportMode::EBike); });
    connect(btnRun, &QPushButton::clicked, this, [this](){ onModeSearch(TransportMode::Run); });
    connect(btnBus, &QPushButton::clicked, this, [this](){ onModeSearch(TransportMode::Bus); });

    panelLayout->addSpacing(15);
    
    // 结果区
    panelLayout->addWidget(createLabel("规划结果"));
    
    routeScrollArea = new QScrollArea();
    routeScrollArea->setWidgetResizable(true);
    routeScrollArea->setStyleSheet("QScrollArea { border: none; background-color: transparent; }" + SCROLL_STYLE);
    
    routePanelWidget = new QWidget();
    routePanelWidget->setStyleSheet("background-color: transparent;");
    routePanelLayout = new QVBoxLayout(routePanelWidget);
    routePanelLayout->setAlignment(Qt::AlignTop);
    routePanelLayout->setContentsMargins(0,0,0,0);
    routePanelLayout->setSpacing(12);
    
    routeScrollArea->setWidget(routePanelWidget);
    panelLayout->addWidget(routeScrollArea, 1);

    // 底部工具按钮
    openEditorBtn = new QPushButton("🛠️ 进入地图编辑器");
    openEditorBtn->setCursor(Qt::PointingHandCursor);
    openEditorBtn->setStyleSheet(
        "QPushButton { "
        "    background-color: #F2F2F7; "
        "    color: #007AFF; "
        "    border: none; " 
        "    border-radius: 10px; "
        "    padding: 12px; "
        "    font-weight: bold; "
        "    font-size: 14px; "
        "} "
        "QPushButton:hover { background-color: #E5E5EA; }"
        "QPushButton:pressed { background-color: #D1D1D6; }"
    );
    panelLayout->addWidget(openEditorBtn);

    statusLabel = new QLabel("Ready to go.");
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet("color: #8E8E93; font-size: 11px; border: none; background: transparent;");
    panelLayout->addWidget(statusLabel);

    mainLayout->addWidget(controlPanel);
    mainLayout->addWidget(mapWidget, 1);
}

void MainWindow::onMapNodeClicked(int nodeId, QString name, bool isLeftClick)
{
    if (isLeftClick) {
        startEdit->setText(name);
        currentStartId = nodeId;
        statusLabel->setText("已设置起点");
    } else {
        endEdit->setText(name);
        currentEndId = nodeId;
        statusLabel->setText("已设置终点");
    }
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
        QMessageBox::warning(this, "提示", "请先在地图上选择起点(左键)和终点(右键)！");
        return;
    }

    Weather w = Weather::Sunny;
    int idx = weatherCombo->currentIndex();
    if (idx == 1) w = Weather::Rainy;
    if (idx == 2) w = Weather::Snowy;

    QTime curTime(spinCurrHour->value(), spinCurrMin->value());
    QTime clsTime(spinClassHour->value(), spinClassMin->value());
    
    bool checkLate = lateCheckToggle->isChecked();

    statusLabel->setText("正在规划路线...");
    
    resetAllButtonStyles();

    PathRecommendation rec = model->getSpecificRoute(
        currentStartId, currentEndId, mode, w, curTime, clsTime, checkLate
    );

    QPushButton* currentBtn = nullptr;
    switch (mode) {
        case TransportMode::Walk: currentBtn = btnWalk; break;
        case TransportMode::SharedBike: currentBtn = btnBike; break;
        case TransportMode::EBike: currentBtn = btnEBike; break;
        case TransportMode::Run: currentBtn = btnRun; break;
        case TransportMode::Bus: currentBtn = btnBus; break;
    }

    if (currentBtn) {
        bool showRedAlert = rec.isLate;
        if (rec.pathNodeIds.isEmpty()) showRedAlert = false;
        updateButtonStyle(currentBtn, true, showRedAlert);
    }

    QVector<PathRecommendation> results;
    if (!rec.pathNodeIds.isEmpty()) {
        results.append(rec);
        QString statusText = QString("规划成功: %1").arg(rec.typeName);
        if (checkLate) {
            if (rec.isLate) statusText += " (⚠️ 预计迟到)";
            else statusText += " (✅ 时间充裕)";
        }
        statusLabel->setText(statusText);
    } else {
        statusLabel->setText("该方式无可行路线");
        QMessageBox::information(this, "提示", "该交通方式下无可行路线。\n原因可能是：\n1. 雪天禁行\n2. 楼梯阻断了车辆\n3. 孤立节点");
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