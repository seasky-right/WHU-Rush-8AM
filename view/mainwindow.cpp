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
#include <QtWidgets/QFrame>
#include <QtWidgets/QButtonGroup>

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
//  【资源】内嵌 SVG 图标 (浅灰色箭头)
// ==========================================================================

// 下箭头 (浅灰色)
const QString ICON_CHEVRON_DOWN = 
    "url(\"data:image/svg+xml;charset=utf-8,"
    "<svg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 24 24' fill='none' stroke='%238E8E93' stroke-width='2.5' stroke-linecap='round' stroke-linejoin='round'>"
    "<polyline points='6 9 12 15 18 9'></polyline>"
    "</svg>\")";

// 上箭头 (浅灰色)
const QString ICON_CHEVRON_UP = 
    "url(\"data:image/svg+xml;charset=utf-8,"
    "<svg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 24 24' fill='none' stroke='%238E8E93' stroke-width='2.5' stroke-linecap='round' stroke-linejoin='round'>"
    "<polyline points='18 15 12 9 6 15'></polyline>"
    "</svg>\")";

// --------------------------------------------------------------------------
//  现代iOS风格滚动条样式
// --------------------------------------------------------------------------
const QString SCROLL_STYLE = 
    "QScrollBar:vertical { background: transparent; width: 6px; margin: 2px; border-radius: 3px; }"
    "QScrollBar::handle:vertical { background: rgba(0,0,0,0.15); min-height: 30px; border-radius: 3px; }"
    "QScrollBar::handle:vertical:hover { background: rgba(0,0,0,0.25); }"
    "QScrollBar::handle:vertical:pressed { background: rgba(0,0,0,0.35); }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
    "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
    "QScrollBar:horizontal { background: transparent; height: 6px; margin: 2px; border-radius: 3px; }"
    "QScrollBar::handle:horizontal { background: rgba(0,0,0,0.15); min-width: 30px; border-radius: 3px; }"
    "QScrollBar::handle:horizontal:hover { background: rgba(0,0,0,0.25); }"
    "QScrollBar::handle:horizontal:pressed { background: rgba(0,0,0,0.35); }"
    "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
    "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }";

// --------------------------------------------------------------------------
//  悬浮卡片样式 (圆角矩形白色卡片+阴影)
// --------------------------------------------------------------------------
const QString CARD_STYLE = 
    "QFrame { "
    "    background-color: #FFFFFF; "
    "    border-radius: 16px; "
    "    border: none; "
    "}";

// --------------------------------------------------------------------------
//  iOS风格输入框样式
// --------------------------------------------------------------------------
const QString INPUT_STYLE = 
    "QLineEdit { "
    "    background-color: #F5F5F7; "
    "    border: none; "
    "    border-radius: 10px; "
    "    padding: 10px 14px; "
    "    font-size: 14px; "
    "    color: #1C1C1E; "
    "}"
    "QLineEdit:focus { "
    "    background-color: #FFFFFF; "
    "    border: 2px solid #007AFF; "
    "}";

// --------------------------------------------------------------------------
//  iOS风格ComboBox样式
// --------------------------------------------------------------------------
const QString COMBO_STYLE = 
    "QComboBox { "
    "    background-color: #F5F5F7; "
    "    border: none; "
    "    border-radius: 10px; "
    "    padding: 10px 14px; "
    "    font-size: 14px; "
    "    color: #1C1C1E; "
    "    min-height: 20px; "
    "}"
    "QComboBox:hover { background-color: #EBEBED; }"
    "QComboBox:on { background-color: #FFFFFF; border: 2px solid #007AFF; }"
    "QComboBox::drop-down { "
    "    border: none; "
    "    width: 30px; "
    "}"
    "QComboBox::down-arrow { "
    "    image: " + ICON_CHEVRON_DOWN + "; "
    "    width: 12px; height: 12px; "
    "}"
    "QComboBox QAbstractItemView { "
    "    background-color: #FFFFFF; "
    "    border: 1px solid #E5E5EA; "
    "    border-radius: 10px; "
    "    selection-background-color: #007AFF; "
    "    selection-color: white; "
    "    padding: 5px; "
    "}";

// ============================================================
// 主窗口构造函数
// 初始化整个应用的UI和数据模型
// ============================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 设置窗口标题和大小
    this->setWindowTitle("WHU Morning Rush - 早八冲锋号");
    this->resize(1400, 900);

    // 创建数据模型和地图组件
    model = new GraphModel();
    mapWidget = new MapWidget(this);
    
    // 配置地图显示模式
    mapWidget->setEditMode(EditMode::None);  // 主界面不可编辑
    mapWidget->setShowEdges(false);          // 不显示所有边
    mapWidget->setNodeSizeMultiplier(2.0);   // 节点放大2倍

    // 创建界面UI
    setupUi();

    // 连接信号与槽
    connect(mapWidget, &MapWidget::nodeClicked, this, &MainWindow::onMapNodeClicked);
    connect(openEditorBtn, &QPushButton::clicked, this, &MainWindow::onOpenEditor);

    // 获取应用程序所在目录
    QString appDir = QCoreApplication::applicationDirPath();
    
    // 加载地图数据和校车时刻表
    bool mapLoaded = model->loadData(appDir + "/Data/nodes.txt", appDir + "/Data/edges.txt");
    bool scheduleLoaded = model->loadSchedule(appDir + "/Data/bus_schedule.csv");

    // 根据加载结果更新界面
    if (mapLoaded)
    {
        // 在地图上绘制节点和边
        mapWidget->drawMap(model->getAllNodes(), model->getAllEdges());
        
        // 设置背景地图图片
        mapWidget->setBackgroundImage(appDir + "/Data/map.png");
        
        // 根据时刻表加载情况显示不同状态
        if (scheduleLoaded)
        {
            statusLabel->setText("地图与时刻表加载成功");
        }
        else
        {
            statusLabel->setText("注意：校车时刻表加载失败");
        }
    }
    else
    {
        statusLabel->setText("数据加载失败");
    }
}

// ============================================================
// 析构函数
// ============================================================
MainWindow::~MainWindow()
{
    delete model;
}

// 辅助函数：创建美化的时间选择器 (时 : 分) - iOS风格
QWidget* createTimeSpinner(QSpinBox*& spinHour, QSpinBox*& spinMin, QTime defaultTime) {
    QWidget* container = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto setupSpin = [](QSpinBox* spin, int max) {
        spin->setRange(0, max);
        spin->setAlignment(Qt::AlignCenter);
        spin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
        
        // iOS风格时间选择器 - 浅灰色背景、白色边框按钮
        spin->setStyleSheet(
            "QSpinBox { "
            "    background-color: #F5F5F7; "
            "    border: none; "
            "    border-radius: 10px; "
            "    padding: 8px 2px; "
            "    font-size: 16px; "
            "    color: #1C1C1E; "
            "    font-weight: 600; "
            "    min-width: 50px; "
            "}"
            "QSpinBox:focus { "
            "    background-color: #FFFFFF; "
            "    border: 2px solid #007AFF; "
            "}"
            
            // 上下按钮区域 - 浅灰色背景白色边框
            "QSpinBox::up-button { "
            "    subcontrol-origin: border; "
            "    subcontrol-position: top right; "
            "    width: 22px; "
            "    height: 14px; "
            "    background: #F0F0F2; "
            "    border: 1px solid #FFFFFF; "
            "    border-radius: 4px; "
            "    margin: 2px 2px 0 0; "
            "}"
            "QSpinBox::up-button:hover { "
            "    background: #E8E8EA; "
            "}"
            "QSpinBox::up-button:pressed { "
            "    background: #D8D8DA; "
            "}"
            
            "QSpinBox::down-button { "
            "    subcontrol-origin: border; "
            "    subcontrol-position: bottom right; "
            "    width: 22px; "
            "    height: 14px; "
            "    background: #F0F0F2; "
            "    border: 1px solid #FFFFFF; "
            "    border-radius: 4px; "
            "    margin: 0 2px 2px 0; "
            "}"
            "QSpinBox::down-button:hover { "
            "    background: #E8E8EA; "
            "}"
            "QSpinBox::down-button:pressed { "
            "    background: #D8D8DA; "
            "}"
            
            // 使用浅灰色 SVG 图标
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

    // 冒号分隔符 - 透明背景避免暗色区域
    QLabel* sep = new QLabel(":");
    sep->setFixedWidth(12);
    sep->setAlignment(Qt::AlignCenter);
    sep->setStyleSheet(
        "QLabel { "
        "    font-size: 18px; "
        "    font-weight: bold; "
        "    color: #8E8E93; "
        "    background: transparent; "
        "    border: none; "
        "    padding: 0px; "
        "    margin: 0px; "
        "}"
    );

    layout->addWidget(spinHour, 1);
    layout->addWidget(sep, 0);
    layout->addWidget(spinMin, 1);
    
    return container;
}

void MainWindow::setupUi() {
    QWidget* centralWidget = new QWidget(this);
    this->setCentralWidget(centralWidget);
    // 浅灰色背景底色
    centralWidget->setStyleSheet("background-color: #F2F2F7;"); 

    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 使用 Splitter 分割左右 (左边控制，右边地图)
    QSplitter* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);

    // === 左侧控制面板 (白色背景上的悬浮卡片) ===
    QWidget* leftWidget = new QWidget();
    leftWidget->setMinimumWidth(340);
    leftWidget->setMaximumWidth(400);
    leftWidget->setStyleSheet("background-color: #F2F2F7; border: none;");
    
    QVBoxLayout* panelLayout = new QVBoxLayout(leftWidget);
    panelLayout->setContentsMargins(16, 16, 16, 16);
    panelLayout->setSpacing(12);

    // 标题
    QLabel* titleLabel = new QLabel("WHU Rush 🚀");
    titleLabel->setStyleSheet(
        "font-size: 24px; "
        "font-weight: 900; "
        "color: #1C1C1E; "
        "background: transparent; "
        "padding: 4px 0px 8px 4px;"
    );
    panelLayout->addWidget(titleLabel);

    // ========================================
    // 卡片1: 路线设定 (圆角悬浮窗)
    // ========================================
    QFrame* cardRoute = new QFrame();
    cardRoute->setStyleSheet(CARD_STYLE);
    QGraphicsDropShadowEffect* shadow1 = new QGraphicsDropShadowEffect();
    shadow1->setBlurRadius(20);
    shadow1->setColor(QColor(0, 0, 0, 25));
    shadow1->setOffset(0, 4);
    cardRoute->setGraphicsEffect(shadow1);
    
    QVBoxLayout* routeLayout = new QVBoxLayout(cardRoute);
    routeLayout->setContentsMargins(16, 14, 16, 14);
    routeLayout->setSpacing(10);
    
    QLabel* routeTitle = new QLabel("📍 路线设定");
    routeTitle->setStyleSheet("font-size: 15px; font-weight: 700; color: #1C1C1E; background: transparent;");
    routeLayout->addWidget(routeTitle);
    
    startEdit = new QLineEdit(); 
    startEdit->setPlaceholderText("🟢 点击地图选择起点"); 
    startEdit->setReadOnly(true);
    startEdit->setStyleSheet(INPUT_STYLE);
    
    endEdit = new QLineEdit(); 
    endEdit->setPlaceholderText("🔴 点击地图选择终点"); 
    endEdit->setReadOnly(true);
    endEdit->setStyleSheet(INPUT_STYLE);
    
    routeLayout->addWidget(startEdit);
    routeLayout->addWidget(endEdit);

    // 途经点控件 - 按钮样式的切换开关
    QHBoxLayout* wpLayout = new QHBoxLayout();
    wpLayout->setSpacing(8);
    
    waypointCheck = new QCheckBox("途经点模式");
    waypointCheck->setCursor(Qt::PointingHandCursor);
    waypointCheck->setStyleSheet(
        "QCheckBox { "
        "    background-color: #F5F5F7; "
        "    border-radius: 8px; "
        "    padding: 8px 12px; "
        "    font-size: 13px; "
        "    color: #1C1C1E; "
        "    font-weight: 500; "
        "    spacing: 6px; "
        "}"
        "QCheckBox:checked { "
        "    background-color: #007AFF; "
        "    color: white; "
        "}"
        "QCheckBox:hover { "
        "    background-color: #E5E5EA; "
        "}"
        "QCheckBox:checked:hover { "
        "    background-color: #0056B3; "
        "}"
        "QCheckBox::indicator { "
        "    width: 16px; height: 16px; "
        "    border-radius: 4px; "
        "    border: 2px solid #C7C7CC; "
        "    background: white; "
        "}"
        "QCheckBox::indicator:checked { "
        "    background-color: white; "
        "    border: 2px solid white; "
        "    image: url(data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxMiIgaGVpZ2h0PSIxMiIgdmlld0JveD0iMCAwIDI0IDI0IiBmaWxsPSJub25lIiBzdHJva2U9IiMwMDdBRkYiIHN0cm9rZS13aWR0aD0iNCIgc3Ryb2tlLWxpbmVjYXA9InJvdW5kIiBzdHJva2UtbGluZWpvaW49InJvdW5kIj48cG9seWxpbmUgcG9pbnRzPSIyMCA2IDkgMTcgNCAxMiI+PC9wb2x5bGluZT48L3N2Zz4=); "
        "}"
    );
    
    QPushButton* btnClearWp = new QPushButton("清空");
    btnClearWp->setCursor(Qt::PointingHandCursor);
    btnClearWp->setStyleSheet(
        "QPushButton { "
        "    background-color: #F5F5F7; "
        "    color: #FF3B30; "
        "    border: none; "
        "    border-radius: 8px; "
        "    padding: 8px 14px; "
        "    font-size: 13px; "
        "    font-weight: 500; "
        "}"
        "QPushButton:hover { background-color: #FFE5E3; }"
        "QPushButton:pressed { background-color: #FFCDD2; }"
    );
    connect(btnClearWp, &QPushButton::clicked, this, [this](){ 
        currentWaypoints.clear(); 
        waypointList->clear(); 
        statusLabel->setText("途经点已清空");
    });
    
    wpLayout->addWidget(waypointCheck, 1);
    wpLayout->addWidget(btnClearWp);
    routeLayout->addLayout(wpLayout);

    waypointList = new QListWidget();
    waypointList->setFixedHeight(45);
    waypointList->setStyleSheet(
        "QListWidget { "
        "    background: #F5F5F7; "
        "    border: none; "
        "    border-radius: 8px; "
        "    font-size: 11px; "
        "    color: #8E8E93; "
        "}" + SCROLL_STYLE
    );
    routeLayout->addWidget(waypointList);
    panelLayout->addWidget(cardRoute);

    // ========================================
    // 卡片2: 环境参数 (圆角悬浮窗)
    // ========================================
    QFrame* cardEnv = new QFrame();
    cardEnv->setStyleSheet(CARD_STYLE);
    QGraphicsDropShadowEffect* shadow2 = new QGraphicsDropShadowEffect();
    shadow2->setBlurRadius(20);
    shadow2->setColor(QColor(0, 0, 0, 25));
    shadow2->setOffset(0, 4);
    cardEnv->setGraphicsEffect(shadow2);
    
    QVBoxLayout* envMainLayout = new QVBoxLayout(cardEnv);
    envMainLayout->setContentsMargins(16, 14, 16, 14);
    envMainLayout->setSpacing(10);
    
    QLabel* envTitle = new QLabel("⚙️ 环境参数");
    envTitle->setStyleSheet("font-size: 15px; font-weight: 700; color: #1C1C1E; background: transparent;");
    envMainLayout->addWidget(envTitle);
    
    QGridLayout* envLayout = new QGridLayout();
    envLayout->setSpacing(10);
    
    // 天气选择
    weatherCombo = new QComboBox();
    weatherCombo->addItems({"☀️ 晴朗", "🌧️ 下雨", "❄️ 大雪"});
    weatherCombo->setStyleSheet(COMBO_STYLE);
    weatherCombo->setCursor(Qt::PointingHandCursor);
    
    QLabel* weatherLabel = new QLabel("天气");
    weatherLabel->setStyleSheet("font-size: 13px; color: #8E8E93; background: transparent;");
    envLayout->addWidget(weatherLabel, 0, 0);
    envLayout->addWidget(weatherCombo, 0, 1);

    // 出发时间
    QWidget* currTimeW = createTimeSpinner(spinCurrHour, spinCurrMin, QTime::currentTime());
    QLabel* startTimeLabel = new QLabel("出发时间");
    startTimeLabel->setStyleSheet("font-size: 13px; color: #8E8E93; background: transparent;");
    envLayout->addWidget(startTimeLabel, 1, 0);
    envLayout->addWidget(currTimeW, 1, 1);

    // 上课时间
    QWidget* classTimeW = createTimeSpinner(spinClassHour, spinClassMin, QTime(8, 0));
    QLabel* classTimeLabel = new QLabel("上课时间");
    classTimeLabel->setStyleSheet("font-size: 13px; color: #8E8E93; background: transparent;");
    envLayout->addWidget(classTimeLabel, 2, 0);
    envLayout->addWidget(classTimeW, 2, 1);
    
    envMainLayout->addLayout(envLayout);
    
    // 迟到预警按钮 (替代复选框)
    lateCheckToggle = new QCheckBox("⏰ 迟到预警");
    lateCheckToggle->setChecked(true);
    lateCheckToggle->setCursor(Qt::PointingHandCursor);
    lateCheckToggle->setStyleSheet(
        "QCheckBox { "
        "    background-color: #34C759; "
        "    color: white; "
        "    border-radius: 8px; "
        "    padding: 8px 14px; "
        "    font-size: 13px; "
        "    font-weight: 600; "
        "}"
        "QCheckBox:!checked { "
        "    background-color: #F5F5F7; "
        "    color: #8E8E93; "
        "}"
        "QCheckBox:hover { "
        "    opacity: 0.9; "
        "}"
        "QCheckBox::indicator { "
        "    width: 0px; height: 0px; "
        "}"
    );
    envMainLayout->addWidget(lateCheckToggle, 0, Qt::AlignRight);
    
    panelLayout->addWidget(cardEnv);

    // ========================================
    // 卡片3: 出行方式 (圆角悬浮窗 + 滚轮选择)
    // ========================================
    QFrame* cardMode = new QFrame();
    cardMode->setStyleSheet(CARD_STYLE);
    QGraphicsDropShadowEffect* shadow3 = new QGraphicsDropShadowEffect();
    shadow3->setBlurRadius(20);
    shadow3->setColor(QColor(0, 0, 0, 25));
    shadow3->setOffset(0, 4);
    cardMode->setGraphicsEffect(shadow3);
    
    QVBoxLayout* modeMainLayout = new QVBoxLayout(cardMode);
    modeMainLayout->setContentsMargins(16, 14, 16, 14);
    modeMainLayout->setSpacing(10);
    
    QLabel* modeTitle = new QLabel("🚗 出行方式");
    modeTitle->setStyleSheet("font-size: 15px; font-weight: 700; color: #1C1C1E; background: transparent;");
    modeMainLayout->addWidget(modeTitle);
    
    // 横向滚动的出行方式选择区
    QScrollArea* modeScrollArea = new QScrollArea();
    modeScrollArea->setWidgetResizable(true);
    modeScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    modeScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    modeScrollArea->setFixedHeight(70);
    modeScrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
    );
    
    QWidget* modeContainer = new QWidget();
    modeContainer->setStyleSheet("background: transparent;");
    QHBoxLayout* modeLayout = new QHBoxLayout(modeContainer);
    modeLayout->setContentsMargins(0, 0, 0, 0);
    modeLayout->setSpacing(8);
    
    // 创建交通工具按钮 - 紧凑型设计
    auto createModeBtn = [this](const QString& emoji, const QString& name) -> QPushButton* {
        QPushButton* btn = new QPushButton(emoji + "\n" + name);
        btn->setFixedSize(60, 58);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton { "
            "    background-color: #F5F5F7; "
            "    color: #3A3A3C; "
            "    border: none; "
            "    border-radius: 12px; "
            "    font-size: 11px; "
            "    font-weight: 500; "
            "    padding: 4px; "
            "}"
            "QPushButton:hover { "
            "    background-color: #E5E5EA; "
            "}"
            "QPushButton:pressed { "
            "    background-color: #D1D1D6; "
            "}"
        );
        return btn;
    };
    
    btnWalk = createModeBtn("🚶", "步行");
    btnBike = createModeBtn("🚲", "单车");
    btnEBike = createModeBtn("🛵", "电动车");
    btnRun = createModeBtn("🏃", "跑步");
    btnBus = createModeBtn("🚌", "校车");
    
    // 绑定点击事件
    connect(btnWalk, &QPushButton::clicked, this, [=](){ onModeSearch(TransportMode::Walk); });
    connect(btnBike, &QPushButton::clicked, this, [=](){ onModeSearch(TransportMode::SharedBike); });
    connect(btnEBike, &QPushButton::clicked, this, [=](){ onModeSearch(TransportMode::EBike); });
    connect(btnRun, &QPushButton::clicked, this, [=](){ onModeSearch(TransportMode::Run); });
    connect(btnBus, &QPushButton::clicked, this, [=](){ onModeSearch(TransportMode::Bus); });
    
    modeLayout->addWidget(btnWalk);
    modeLayout->addWidget(btnBike);
    modeLayout->addWidget(btnEBike);
    modeLayout->addWidget(btnRun);
    modeLayout->addWidget(btnBus);
    modeLayout->addStretch();
    
    modeScrollArea->setWidget(modeContainer);
    modeMainLayout->addWidget(modeScrollArea);
    
    panelLayout->addWidget(cardMode);

    // ========================================
    // 卡片4: 路径结果 (圆角悬浮窗)
    // ========================================
    QFrame* cardResult = new QFrame();
    cardResult->setStyleSheet(CARD_STYLE);
    QGraphicsDropShadowEffect* shadow4 = new QGraphicsDropShadowEffect();
    shadow4->setBlurRadius(20);
    shadow4->setColor(QColor(0, 0, 0, 25));
    shadow4->setOffset(0, 4);
    cardResult->setGraphicsEffect(shadow4);
    
    QVBoxLayout* resultMainLayout = new QVBoxLayout(cardResult);
    resultMainLayout->setContentsMargins(16, 14, 16, 14);
    resultMainLayout->setSpacing(10);
    
    QLabel* resultTitle = new QLabel("📋 路径方案");
    resultTitle->setStyleSheet("font-size: 15px; font-weight: 700; color: #1C1C1E; background: transparent;");
    resultMainLayout->addWidget(resultTitle);
    
    routeScrollArea = new QScrollArea();
    routeScrollArea->setWidgetResizable(true);
    routeScrollArea->setStyleSheet(
        "QScrollArea { background: #F5F5F7; border: none; border-radius: 10px; }"
        + SCROLL_STYLE
    );
    routePanelWidget = new QWidget();
    routePanelWidget->setStyleSheet("background: transparent;");
    routePanelLayout = new QVBoxLayout(routePanelWidget);
    routePanelLayout->setAlignment(Qt::AlignTop);
    routePanelLayout->setSpacing(8);
    routePanelLayout->setContentsMargins(4, 4, 4, 4);
    routeScrollArea->setWidget(routePanelWidget);
    resultMainLayout->addWidget(routeScrollArea, 1);
    
    panelLayout->addWidget(cardResult, 1);

    // ========================================
    // 底部工具栏
    // ========================================
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(10);
    
    openEditorBtn = new QPushButton("🛠️ 编辑地图");
    openEditorBtn->setCursor(Qt::PointingHandCursor);
    openEditorBtn->setStyleSheet(
        "QPushButton { "
        "    background-color: #FFFFFF; "
        "    color: #007AFF; "
        "    border: none; "
        "    border-radius: 10px; "
        "    padding: 10px 16px; "
        "    font-size: 13px; "
        "    font-weight: 600; "
        "}"
        "QPushButton:hover { background-color: #F0F0F2; }"
        "QPushButton:pressed { background-color: #E5E5EA; }"
    );
    
    statusLabel = new QLabel("Ready");
    statusLabel->setStyleSheet("color: #8E8E93; font-size: 12px; background: transparent;");
    
    bottomLayout->addWidget(openEditorBtn);
    bottomLayout->addWidget(statusLabel, 1);
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

    if (!isSelected) {
        // 未选中：浅灰背景，紧凑型设计
        style = 
            "QPushButton { "
            "    background-color: #F5F5F7; "
            "    color: #3A3A3C; "
            "    border: none; "
            "    border-radius: 12px; "
            "    font-size: 11px; "
            "    font-weight: 500; "
            "    padding: 4px; "
            "}"
            "QPushButton:hover { background-color: #E5E5EA; }"
            "QPushButton:pressed { background-color: #D1D1D6; }";
    } else {
        if (isLate) {
            // 迟到：浅红背景
            style = 
                "QPushButton { "
                "    background-color: #FFEBEE; " 
                "    color: #C62828; "            
                "    border: 2px solid #FFCDD2; "
                "    border-radius: 12px; "
                "    font-size: 11px; "
                "    font-weight: 700; "
                "    padding: 4px; "
                "}";
        } else {
            // 准时：浅绿背景
            style = 
                "QPushButton { "
                "    background-color: #E8F5E9; " 
                "    color: #2E7D32; "            
                "    border: 2px solid #C8E6C9; "
                "    border-radius: 12px; "
                "    font-size: 11px; "
                "    font-weight: 700; "
                "    padding: 4px; "
                "}";
        }
    }
    btn->setStyleSheet(style);
    btn->setCursor(Qt::PointingHandCursor);
}

// ============================================================
// 路径规划的核心函数
// 根据用户选择的交通方式，调用多策略推荐算法
// ============================================================
void MainWindow::onModeSearch(TransportMode mode)
{
    // 检查是否已选择起点和终点
    if (currentStartId == -1)
    {
        QMessageBox::warning(this, "提示", "请先在地图上选择起点和终点！");
        return;
    }
    if (currentEndId == -1)
    {
        QMessageBox::warning(this, "提示", "请先在地图上选择起点和终点！");
        return;
    }

    // 读取天气选择
    Weather selectedWeather = Weather::Sunny;
    int weatherIndex = weatherCombo->currentIndex();
    if (weatherIndex == 1)
    {
        selectedWeather = Weather::Rainy;
    }
    if (weatherIndex == 2)
    {
        selectedWeather = Weather::Snowy;
    }
    
    // 更新地图天气效果
    mapWidget->setWeather(selectedWeather);

    // 读取当前时间和上课时间
    QTime currentTime(spinCurrHour->value(), spinCurrMin->value());
    QTime classTime(spinClassHour->value(), spinClassMin->value());
    
    // 读取是否检查迟到
    bool checkLate = lateCheckToggle->isChecked();

    // 更新状态提示
    statusLabel->setText("正在规划多策略路线...");
    resetAllButtonStyles();

    // 调用核心算法：多策略路径推荐
    // 会返回最多3种方案：极限冲刺、懒人养生、经济适用
    QVector<PathRecommendation> results = model->getMultiStrategyRoutes(
        currentStartId,
        currentEndId,
        currentWaypoints,
        mode,
        selectedWeather,
        currentTime,
        classTime,
        checkLate
    );

    // 更新按钮样式：高亮当前交通方式
    QPushButton* currentModeButton = nullptr;
    if (mode == TransportMode::Walk)
    {
        currentModeButton = btnWalk;
    }
    else if (mode == TransportMode::SharedBike)
    {
        currentModeButton = btnBike;
    }
    else if (mode == TransportMode::EBike)
    {
        currentModeButton = btnEBike;
    }
    else if (mode == TransportMode::Run)
    {
        currentModeButton = btnRun;
    }
    else if (mode == TransportMode::Bus)
    {
        currentModeButton = btnBus;
    }

    // 判断是否所有路线都迟到（全红告警）
    if (currentModeButton)
    {
        bool allRoutesLate = false;
        if (!results.isEmpty())
        {
            allRoutesLate = true;
            for (const auto& route : results)
            {
                if (!route.isLate)
                {
                    allRoutesLate = false;
                    break;
                }
            }
        }
        
        updateButtonStyle(currentModeButton, true, allRoutesLate);
    }

    // 根据结果更新状态
    if (results.isEmpty())
    {
        statusLabel->setText("无可行路线");
        QMessageBox::information(this, "提示", "无法找到路径。\n请检查是否被雪天/楼梯阻断，或节点不连通。");
    }
    else
    {
        QString message = QString("规划完成，找到 %1 种方案").arg(results.size());
        statusLabel->setText(message);
    }
    
    // 在右侧面板显示推荐路线
    displayRouteRecommendations(results);
}

// ============================================================
// 显示路径推荐结果
// 在右侧面板创建路线按钮，并高亮第一条路线
// ============================================================
void MainWindow::displayRouteRecommendations(const QVector<PathRecommendation>& recommendations)
{
    // 清空之前的推荐结果
    clearRoutePanel();
    
    // 保存当前推荐列表
    currentRecommendations = recommendations;
    
    // 为每条推荐路线创建一个按钮
    for (int i = 0; i < recommendations.size(); ++i)
    {
        // 创建路线按钮组件
        RouteButton* routeBtn = new RouteButton(recommendations[i]);
        routePanelLayout->addWidget(routeBtn);
        
        // 连接点击事件：选中这条路线
        connect(routeBtn, &QPushButton::clicked, this, [this, i]() {
            onRouteButtonClicked(i);
        });
        
        // 连接悬停事件：预览路线
        connect(routeBtn, &RouteButton::routeHovered, this, &MainWindow::onRouteHovered);
        connect(routeBtn, &RouteButton::routeUnhovered, this, &MainWindow::onRouteUnhovered);
    }
    
    // 默认高亮显示第一条推荐路线
    if (!recommendations.isEmpty())
    {
        mapWidget->highlightPath(recommendations[0].pathNodeIds, 1.0);
    }
    else
    {
        mapWidget->clearPathHighlight();
    }
}

// ============================================================
// 清空路线推荐面板
// ============================================================
void MainWindow::clearRoutePanel()
{
    QLayoutItem* item;
    
    // 逐个取出布局中的组件并删除
    while ((item = routePanelLayout->takeAt(0)) != nullptr)
    {
        delete item->widget();
        delete item;
    }
    
    routeButtons.clear();
}

// ============================================================
// 用户点击某条路线按钮
// ============================================================
void MainWindow::onRouteButtonClicked(int routeIndex)
{
    // 检查索引是否有效
    if (routeIndex < 0)
    {
        return;
    }
    if (routeIndex >= currentRecommendations.size())
    {
        return;
    }
    
    // 获取选中的路线
    const auto& selectedRoute = currentRecommendations[routeIndex];
    
    // 在地图上完全高亮这条路线
    mapWidget->highlightPath(selectedRoute.pathNodeIds, 1.0);
    
    // 更新状态栏
    statusLabel->setText("已选择: " + selectedRoute.typeName);
}

// ============================================================
// 鼠标悬停在路线按钮上
// ============================================================
void MainWindow::onRouteHovered(const PathRecommendation& recommendation)
{
    // 半透明预览这条路线
    mapWidget->highlightPath(recommendation.pathNodeIds, 0.8);
}

// ============================================================
// 鼠标离开路线按钮
// ============================================================
void MainWindow::onRouteUnhovered()
{
    // 可以在这里恢复之前选中的路线显示
}

// ============================================================
// 打开地图编辑器
// ============================================================
void MainWindow::onOpenEditor()
{
    // 创建编辑器窗口
    EditorWindow* editor = new EditorWindow(this->model, this);
    
    // 连接数据变更信号：编辑器修改地图后通知主窗口更新
    connect(editor, &EditorWindow::dataChanged, this, &MainWindow::onMapDataChanged);
    
    // 设置为模态窗口：打开编辑器时主窗口暂停交互
    editor->setWindowModality(Qt::WindowModal);
    
    // 显示编辑器窗口
    editor->show();
}

// ============================================================
// 地图数据被编辑器修改后的回调
// ============================================================
void MainWindow::onMapDataChanged()
{
    // 重新绘制地图
    mapWidget->drawMap(model->getAllNodes(), model->getAllEdges());
    
    // 更新状态提示
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
        // 检查是否开启了途经点模式
        if (waypointCheck && waypointCheck->isChecked())
        {
            // === 途经点模式：左键添加途经点 ===
            // 避免重复添加
            if (!currentWaypoints.contains(nodeId)) {
                currentWaypoints.append(nodeId);
                waypointList->addItem(QString("📌 %1").arg(name));
                statusLabel->setText(QString("添加途经点: %1").arg(name));
            } else {
                statusLabel->setText(QString("途经点 %1 已存在").arg(name));
            }
            return; // 途经点模式下不设置起点
        }
        
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