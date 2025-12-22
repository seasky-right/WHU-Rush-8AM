#include "EditorWindow.h"
#include <QtWidgets/QMessageBox>
#include <QtCore/QCoreApplication>
#include <cmath>

// ============================================================
// 地图编辑器窗口构造函数
// ============================================================
EditorWindow::EditorWindow(GraphModel* sharedModel, QWidget *parent)
    : QMainWindow(parent), model(sharedModel)
{
    // 设置窗口标题和大小
    this->setWindowTitle("地图编辑器 - 极速模式");
    this->resize(1200, 800);

    // 创建独立的地图组件，专用于编辑
    mapWidget = new MapWidget(this);

    // 开启编辑功能：允许拖拽节点
    mapWidget->setEditable(true);
    
    // 显示所有节点和路线
    mapWidget->setShowGhostNodes(true);  // 包括幽灵节点
    mapWidget->setShowEdges(true);       // 显示所有边
    mapWidget->setNodeSizeMultiplier(1.0); // 节点正常大小
    
    // 设置背景地图
    QString appDir = QCoreApplication::applicationDirPath();
    mapWidget->setBackgroundImage(appDir + "/Data/map.png");
    
    // 创建界面UI
    setupUi();

    // =========================================================
    // 信号连接（关键区：防止Crash）
    // =========================================================
    
    // 1. 点击节点 -> 显示属性面板
    connect(mapWidget, &MapWidget::nodeEditClicked, this, &EditorWindow::onNodeEditClicked);
    
    // 2. 点击空白 -> 新建节点
    // 使用QueuedConnection防止在事件处理中删除对象
    connect(mapWidget, &MapWidget::emptySpaceClicked, this, &EditorWindow::onEmptySpaceClicked, Qt::QueuedConnection);
    
    // 3. 连线请求 -> 自动连接两个节点
    // 关键修复：必须加Qt::QueuedConnection！
    // 否则点击的一瞬间MapWidget的item被删除，导致Crash
    connect(mapWidget, &MapWidget::edgeConnectionRequested, this, &EditorWindow::onEdgeConnectionRequested, Qt::QueuedConnection);
    
    // 4. 拖拽节点 -> 移动并保存
    connect(mapWidget, &MapWidget::nodeMoved, this, &EditorWindow::onNodeMoved, Qt::QueuedConnection);
    
    // 5. 撤销操作
    connect(mapWidget, &MapWidget::undoRequested, this, &EditorWindow::onUndoRequested, Qt::QueuedConnection);

    // 初始化地图显示
    refreshMap();
    
    // 默认进入浏览模式
    mapWidget->setEditMode(EditMode::None);
}

// ============================================================
// 刷新地图显示
// 从模型中加载最新的节点和边数据
// ============================================================
void EditorWindow::refreshMap()
{
    if (model && mapWidget)
    {
        mapWidget->drawMap(model->getAllNodes(), model->getAllEdges());
    }
}

void EditorWindow::setupUi() {
    QWidget* centralWidget = new QWidget(this);
    this->setCentralWidget(centralWidget);

    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // =========================================================
    // 左侧：工具栏 + 地图
    // =========================================================
    QWidget* leftContainer = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(10, 10, 10, 10);
    leftLayout->setSpacing(10);

    // --- 工具栏 ---
    QFrame* toolBar = new QFrame();
    toolBar->setFixedHeight(60);
    toolBar->setStyleSheet("QFrame { background-color: #FFFFFF; border-radius: 8px; border: 1px solid #E5E5EA; }");
    QHBoxLayout* toolLayout = new QHBoxLayout(toolBar);
    toolLayout->setContentsMargins(10, 5, 10, 5);
    toolLayout->setSpacing(15);

    auto setupToolBtn = [](QPushButton* btn, QString text) {
        btn->setText(text);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        btn->setFixedHeight(36);
        btn->setStyleSheet(
            "QPushButton { background-color: #F2F2F7; color: #1C1C1E; border: none; border-radius: 6px; padding: 0 15px; font-weight: bold; } "
            "QPushButton:hover { background-color: #E5E5EA; } "
            "QPushButton:checked { background-color: #007AFF; color: white; }"
        );
    };

    QPushButton* btnBrowse = new QPushButton(); setupToolBtn(btnBrowse, "👀 浏览/移动"); btnBrowse->setChecked(true);
    QPushButton* btnConnect = new QPushButton(); setupToolBtn(btnConnect, "🔗 极速连线");
    QPushButton* btnAddBuild = new QPushButton(); setupToolBtn(btnAddBuild, "🏢 新建建筑");
    QPushButton* btnAddRoad = new QPushButton(); setupToolBtn(btnAddRoad, "👻 新建路口");

    modeGroup = new QButtonGroup(this);
    modeGroup->addButton(btnBrowse, 0); 
    modeGroup->addButton(btnConnect, 1);
    modeGroup->addButton(btnAddBuild, 2);
    modeGroup->addButton(btnAddRoad, 3);
    connect(modeGroup, &QButtonGroup::idClicked, this, &EditorWindow::onModeChanged);

    toolLayout->addWidget(btnBrowse);
    toolLayout->addWidget(btnConnect);
    toolLayout->addWidget(btnAddBuild);
    toolLayout->addWidget(btnAddRoad);
    
    QFrame* vLine = new QFrame(); vLine->setFrameShape(QFrame::VLine); vLine->setFrameShadow(QFrame::Sunken); toolLayout->addWidget(vLine);

    QPushButton* btnUndo = new QPushButton("↩️ 撤销");
    btnUndo->setStyleSheet("QPushButton { background-color: #F2F2F7; border-radius: 6px; padding: 6px 12px; border: 1px solid #D1D1D6; }");
    connect(btnUndo, &QPushButton::clicked, this, &EditorWindow::onUndoRequested);
    toolLayout->addWidget(btnUndo);

    toolLayout->addStretch();
    statusLabel = new QLabel("就绪 (修改即时生效)");
    statusLabel->setStyleSheet("color: #007AFF; font-weight: bold; font-size: 12px;");
    toolLayout->addWidget(statusLabel);

    leftLayout->addWidget(toolBar);
    leftLayout->addWidget(mapWidget);

    setupRightPanel();

    mainLayout->addWidget(leftContainer, 1);
    mainLayout->addWidget(rightPanelStack);
}

void EditorWindow::setupRightPanel() {
    rightPanelStack = new QStackedWidget();
    rightPanelStack->setFixedWidth(300);
    rightPanelStack->setStyleSheet("background-color: #FFFFFF; border-left: 1px solid #E5E5EA;");
    
    emptyPanel = new QWidget();
    QVBoxLayout* emptyLayout = new QVBoxLayout(emptyPanel);
    QLabel* emptyLabel = new QLabel("选中元素以编辑\n(支持即时修改)");
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet("color: #8E8E93; font-size: 14px;");
    emptyLayout->addWidget(emptyLabel);
    rightPanelStack->addWidget(emptyPanel);

    // --- 节点属性页 ---
    nodePropPanel = new QWidget();
    QVBoxLayout* nodeLayout = new QVBoxLayout(nodePropPanel);
    nodeLayout->setAlignment(Qt::AlignTop);
    nodeLayout->setSpacing(15);
    nodeLayout->setContentsMargins(20, 30, 20, 20);

    QLabel* nodeTitle = new QLabel("节点属性");
    nodeTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #1C1C1E;");
    nodeLayout->addWidget(nodeTitle);

    nodeCoordLabel = new QLabel("坐标: (0, 0)");
    nodeCoordLabel->setStyleSheet("color: #8E8E93; font-family: monospace;");
    nodeLayout->addWidget(nodeCoordLabel);

    nodeLayout->addWidget(new QLabel("名称:"));
    nodeNameEdit = new QLineEdit();
    nodeNameEdit->setPlaceholderText("输入名称...");
    // 即时保存
    connect(nodeNameEdit, &QLineEdit::textEdited, this, &EditorWindow::onLiveNodePropChanged);
    nodeLayout->addWidget(nodeNameEdit);
    
    nodeLayout->addWidget(new QLabel("海拔 (Z):"));
    nodeZEdit = new QLineEdit();
    nodeZEdit->setPlaceholderText("30.0");
    connect(nodeZEdit, &QLineEdit::textEdited, this, &EditorWindow::onLiveNodePropChanged);
    nodeLayout->addWidget(nodeZEdit);

    nodeLayout->addWidget(new QLabel("功能分类:"));
    nodeCatCombo = new QComboBox();
    nodeCatCombo->addItems({"None", "Dorm", "Canteen", "Service", "Square", "Gate", "Road", 
                           "Park", "Shop", "Playground", "Landmark", "Lake", "Building", 
                           "Classroom", "Hotel", "BusStation"});
    connect(nodeCatCombo, &QComboBox::currentIndexChanged, this, &EditorWindow::onLiveNodePropChanged);
    nodeLayout->addWidget(nodeCatCombo);

    nodeLayout->addWidget(new QLabel("描述/备注:"));
    nodeDescEdit = new QLineEdit();
    connect(nodeDescEdit, &QLineEdit::textEdited, this, &EditorWindow::onLiveNodePropChanged);
    nodeLayout->addWidget(nodeDescEdit);

    QPushButton* btnDelNode = new QPushButton("🗑️ 删除节点");
    btnDelNode->setStyleSheet("background-color: #FF3B30; color: white; padding: 8px; border-radius: 5px; margin-top: 20px;");
    connect(btnDelNode, &QPushButton::clicked, this, &EditorWindow::onDeleteNode);
    nodeLayout->addWidget(btnDelNode);

    nodeLayout->addStretch();
    rightPanelStack->addWidget(nodePropPanel);

    // --- 边属性页 ---
    edgePropPanel = new QWidget();
    QVBoxLayout* edgeLayout = new QVBoxLayout(edgePropPanel);
    edgeLayout->setAlignment(Qt::AlignTop);
    edgeLayout->setSpacing(15);
    edgeLayout->setContentsMargins(20, 30, 20, 20);

    QLabel* edgeTitle = new QLabel("道路属性");
    edgeTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #1C1C1E;");
    edgeLayout->addWidget(edgeTitle);

    edgeInfoLabel = new QLabel("连接: A <-> B");
    edgeInfoLabel->setStyleSheet("color: #8E8E93;");
    edgeLayout->addWidget(edgeInfoLabel);

    edgeLayout->addWidget(new QLabel("道路类型:"));
    edgeTypeCombo = new QComboBox();
    edgeTypeCombo->addItems({"普通道路 (Normal)", "主干道 (Main)", "小径 (Path)", "室内 (Indoor)", "楼梯 (Stairs)"});
    connect(edgeTypeCombo, &QComboBox::currentIndexChanged, this, &EditorWindow::onLiveEdgePropChanged);
    edgeLayout->addWidget(edgeTypeCombo);

    edgeSlopeCheck = new QCheckBox("⚠️ 是陡坡/爬坡 (Slope)");
    edgeSlopeCheck->setStyleSheet("color: #FF9500; font-weight: bold;");
    connect(edgeSlopeCheck, &QCheckBox::toggled, this, &EditorWindow::onLiveEdgePropChanged);
    edgeLayout->addWidget(edgeSlopeCheck);

    edgeLayout->addWidget(new QLabel("道路名称:"));
    edgeNameEdit = new QLineEdit();
    connect(edgeNameEdit, &QLineEdit::textEdited, this, &EditorWindow::onLiveEdgePropChanged);
    edgeLayout->addWidget(edgeNameEdit);

    edgeLayout->addWidget(new QLabel("描述:"));
    edgeDescEdit = new QLineEdit();
    connect(edgeDescEdit, &QLineEdit::textEdited, this, &EditorWindow::onLiveEdgePropChanged);
    edgeLayout->addWidget(edgeDescEdit);

    edgeDisconnectBtn = new QPushButton("❌ 断开连接");
    edgeDisconnectBtn->setStyleSheet("background-color: #FF3B30; color: white; padding: 8px; border-radius: 5px; margin-top: 20px;");
    connect(edgeDisconnectBtn, &QPushButton::clicked, this, &EditorWindow::onDisconnectEdge);
    edgeLayout->addWidget(edgeDisconnectBtn);

    edgeLayout->addStretch();
    rightPanelStack->addWidget(edgePropPanel);
}

// =========================================================
//  核心逻辑：自动保存与即时响应
// =========================================================

void EditorWindow::onLiveNodePropChanged() {
    if (currentNodeId == -1) return;
    
    Node n = model->getNode(currentNodeId);
    
    n.name = nodeNameEdit->text();
    n.description = nodeDescEdit->text();
    n.z = nodeZEdit->text().toDouble();
    
    QString selectedCat = nodeCatCombo->currentText();
    n.category = Node::stringToCategory(selectedCat);
    
    model->updateNode(n); // 触发 autoSave
    refreshMap();
    statusLabel->setText("已保存: " + n.name);
}

void EditorWindow::onLiveEdgePropChanged() {
    if (currentEdgeU == -1 || currentEdgeV == -1) return;

    const Edge* existing = model->findEdge(currentEdgeU, currentEdgeV);
    if (!existing) return;

    Edge e = *existing; 
    e.type = static_cast<EdgeType>(edgeTypeCombo->currentIndex());
    e.slope = edgeSlopeCheck->isChecked() ? 0.08 : 0.0;
    e.name = edgeNameEdit->text();
    e.description = edgeDescEdit->text();

    model->addOrUpdateEdge(e); // 触发 autoSave
    refreshMap();
    statusLabel->setText("道路属性已更新");
}

// ============================================================
// 切换编辑模式
// 0=浏览 1=连线 2=新建建筑 3=新建路口
// ============================================================
void EditorWindow::onModeChanged(int id)
{
    // 清除当前选中的边
    mapWidget->setActiveEdge(-1, -1);
    currentNodeId = -1;
    
    // 根据按铞ID决定模式
    EditMode newMode = EditMode::None;
    if (id == 1)
    {
        newMode = EditMode::ConnectEdge;  // 连线模式
    }
    else if (id == 2)
    {
        newMode = EditMode::AddBuilding;  // 添加建筑
    }
    else if (id == 3)
    {
        newMode = EditMode::AddGhost;     // 添加幽灵节点
    }
    
    // 应用新模式
    mapWidget->setEditMode(newMode);
    
    // 切换到空白面板
    rightPanelStack->setCurrentWidget(emptyPanel);
    
    statusLabel->setText("模式切换");
}

// ============================================================
// 点击节点：显示节点属性面板
// ============================================================
void EditorWindow::onNodeEditClicked(int nodeId, bool)
{
    mapWidget->setActiveEdge(-1, -1);
    showNodeProperty(nodeId);
}

// ============================================================
// 点击地图空白处：根据模式创建新节点
// ============================================================
void EditorWindow::onEmptySpaceClicked(double x, double y)
{
    // 清除边选中状态
    mapWidget->setActiveEdge(-1, -1);
    
    // 检查当前编辑模式
    EditMode currentMode = mapWidget->getEditMode();
    
    // 如果是添加建筑或路口模式
    bool isAddBuilding = (currentMode == EditMode::AddBuilding);
    bool isAddGhost = (currentMode == EditMode::AddGhost);
    
    if (isAddBuilding || isAddGhost)
    {
        // 确定节点类型
        NodeType nodeType = NodeType::Visible;
        if (isAddGhost)
        {
            nodeType = NodeType::Ghost;
        }
        
        // 调用模型添加节点
        int newNodeId = model->addNode(x, y, nodeType);
        
        // 刷新地图显示
        refreshMap();
        
        // 显示新节点的属性面板
        showNodeProperty(newNodeId);
        
        statusLabel->setText("新建并保存成功");
    }
}

void EditorWindow::onNodeMoved(int id, double x, double y) {
    Node n = model->getNode(id);
    n.x = x; n.y = y;
    model->updateNode(n); 
    refreshMap();
    if (currentNodeId == id) showNodeProperty(id);
}

void EditorWindow::onUndoRequested() {
    if (model->canUndo()) {
        model->undo(); 
        refreshMap();
        statusLabel->setText("撤销成功");
    }
}

void EditorWindow::showNodeProperty(int id) {
    currentNodeId = id;
    Node n = model->getNode(id);
    
    // 屏蔽信号，防止填充时触发保存
    nodeNameEdit->blockSignals(true);
    nodeDescEdit->blockSignals(true);
    nodeZEdit->blockSignals(true);
    nodeCatCombo->blockSignals(true);

    nodeNameEdit->setText(n.name);
    nodeDescEdit->setText(n.description);
    nodeZEdit->setText(QString::number(n.z)); 
    nodeCoordLabel->setText(QString("(%1, %2)").arg((int)n.x).arg((int)n.y));
    
    if (n.type == NodeType::Ghost) {
        int roadIdx = nodeCatCombo->findText("Road");
        if (roadIdx != -1) nodeCatCombo->setCurrentIndex(roadIdx);
        nodeCatCombo->setEnabled(false);
    } else {
        nodeCatCombo->setEnabled(true);
        QString catStr = Node::categoryToString(n.category);
        int idx = nodeCatCombo->findText(catStr);
        nodeCatCombo->setCurrentIndex(idx != -1 ? idx : 0);
    }

    // 恢复信号
    nodeNameEdit->blockSignals(false);
    nodeDescEdit->blockSignals(false);
    nodeZEdit->blockSignals(false);
    nodeCatCombo->blockSignals(false);
    
    rightPanelStack->setCurrentWidget(nodePropPanel);
}

void EditorWindow::onDeleteNode() {
    if (currentNodeId != -1) {
        mapWidget->setActiveEdge(-1, -1);
        model->deleteNode(currentNodeId);
        currentNodeId = -1;
        refreshMap();
        rightPanelStack->setCurrentWidget(emptyPanel);
    }
}

void EditorWindow::onEdgeConnectionRequested(int idA, int idB) {
    currentEdgeU = idA;
    currentEdgeV = idB;
    
    // 自动连接逻辑
    const Edge* existing = model->findEdge(idA, idB);
    if (!existing) {
        Node a = model->getNode(idA);
        Node b = model->getNode(idB);
        double pixelDist = std::hypot(a.x - b.x, a.y - b.y);
        double realDist = pixelDist * 0.91;

        Edge e;
        e.u = idA; e.v = idB;
        e.distance = realDist;
        e.type = EdgeType::Normal; 
        e.slope = 0.0;             
        e.name = "路";
        e.description = "";
        
        model->addOrUpdateEdge(e); 
        refreshMap();
        statusLabel->setText("自动连线成功");
    }
    
    showEdgePanel(idA, idB);
}

void EditorWindow::showEdgePanel(int u, int v) {
    mapWidget->setActiveEdge(u, v);
    rightPanelStack->setCurrentWidget(edgePropPanel);
    edgeInfoLabel->setText(QString("%1 <-> %2").arg(u).arg(v));
    
    const Edge* e = model->findEdge(u, v);
    if (e) {
        edgeNameEdit->blockSignals(true);
        edgeDescEdit->blockSignals(true);
        edgeSlopeCheck->blockSignals(true);
        edgeTypeCombo->blockSignals(true);

        edgeDisconnectBtn->setEnabled(true);
        edgeNameEdit->setText(e->name);
        edgeDescEdit->setText(e->description);
        edgeSlopeCheck->setChecked(std::abs(e->slope) > 0.01);
        edgeTypeCombo->setCurrentIndex(static_cast<int>(e->type));

        edgeNameEdit->blockSignals(false);
        edgeDescEdit->blockSignals(false);
        edgeSlopeCheck->blockSignals(false);
        edgeTypeCombo->blockSignals(false);
    }
}

void EditorWindow::onDisconnectEdge() {
    if (currentEdgeU != -1) {
        model->deleteEdge(currentEdgeU, currentEdgeV);
        refreshMap();
        rightPanelStack->setCurrentWidget(emptyPanel);
        mapWidget->setActiveEdge(-1, -1);
    }
}

void EditorWindow::onSaveFile() {
    QString appDir = QCoreApplication::applicationDirPath();
    if (model->saveData(appDir + "/Data/nodes.txt", appDir + "/Data/edges.txt")) {
        QMessageBox::information(this, "保存", "所有更改已强制写入磁盘！");
        emit dataChanged();
    }
}