#include "EditorWindow.h"
#include <QtWidgets/QMessageBox>
#include <QtCore/QCoreApplication>
#include <cmath>

EditorWindow::EditorWindow(GraphModel* sharedModel, QWidget *parent)
    : QMainWindow(parent), model(sharedModel)
{
    this->setWindowTitle("地图编辑器 - 管理员模式");
    this->resize(1200, 800);

    // 独立的 MapWidget，专用于编辑
    mapWidget = new MapWidget(this);
    QString appDir = QCoreApplication::applicationDirPath();
    mapWidget->setBackgroundImage(appDir + "/Data/map.png");
    
    // 初始化 UI
    setupUi();

    // =========================================================
    // 【核心修复】修改信号连接，防止"自杀式"崩溃
    // =========================================================

    // 1. 只是显示属性面板，不涉及删除图元，直接连接即可 (AutoConnection)
    connect(mapWidget, &MapWidget::nodeEditClicked, this, &EditorWindow::onNodeEditClicked);
    
    // 2. 【关键】新建节点会触发 refreshMap (清空场景)，必须排队执行 (QueuedConnection)
    //    让当前的鼠标点击事件先执行完，下一轮循环再刷新地图
    connect(mapWidget, &MapWidget::emptySpaceClicked, this, &EditorWindow::onEmptySpaceClicked, Qt::QueuedConnection);
    
    // 3. 只是显示连线面板，安全
    connect(mapWidget, &MapWidget::edgeConnectionRequested, this, &EditorWindow::onEdgeConnectionRequested);
    
    // 4. 【关键】拖拽结束更新位置也会触发 refreshMap，必须排队
    connect(mapWidget, &MapWidget::nodeMoved, this, &EditorWindow::onNodeMoved, Qt::QueuedConnection);
    
    // 5. 撤销操作建议也排队，比较稳妥
    connect(mapWidget, &MapWidget::undoRequested, this, &EditorWindow::onUndoRequested, Qt::QueuedConnection);

    // =========================================================

    // 初始刷新
    refreshMap();
    
    // 默认进入浏览模式
    mapWidget->setEditMode(EditMode::None);
}

void EditorWindow::setupUi() {
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QHBoxLayout* mainLayout = new QHBoxLayout(central);

    // --- 左侧控制栏 ---
    QVBoxLayout* leftLayout = new QVBoxLayout();
    QGroupBox* modeBox = new QGroupBox("工具箱");
    QVBoxLayout* modeLayout = new QVBoxLayout(modeBox);
    
    modeGroup = new QButtonGroup(this);
    QRadioButton* rbView = new QRadioButton("浏览/选择 (None)");
    QRadioButton* rbEdge = new QRadioButton("连线工具 (Edge)");
    QRadioButton* rbBuild = new QRadioButton("新建建筑 (Node)");
    QRadioButton* rbGhost = new QRadioButton("新建路口 (Ghost)");
    
    modeGroup->addButton(rbView, 0);
    modeGroup->addButton(rbEdge, 1);
    modeGroup->addButton(rbBuild, 2);
    modeGroup->addButton(rbGhost, 3);
    rbView->setChecked(true);
    
    modeLayout->addWidget(rbView);
    modeLayout->addWidget(rbEdge);
    modeLayout->addWidget(rbBuild);
    modeLayout->addWidget(rbGhost);
    modeLayout->addStretch();
    connect(modeGroup, &QButtonGroup::idClicked, this, &EditorWindow::onModeChanged);
    
    leftLayout->addWidget(modeBox);

    QPushButton* saveFileBtn = new QPushButton("💾 保存到文件");
    saveFileBtn->setStyleSheet("background-color: #e74c3c; color: white; padding: 10px; font-weight: bold;");
    connect(saveFileBtn, &QPushButton::clicked, this, &EditorWindow::onSaveFile);
    leftLayout->addWidget(saveFileBtn);

    statusLabel = new QLabel("就绪");
    statusLabel->setWordWrap(true);
    leftLayout->addWidget(statusLabel);
    leftLayout->addStretch();

    // --- 右侧属性面板 ---
    setupRightPanel();

    QWidget* leftContainer = new QWidget();
    leftContainer->setLayout(leftLayout);
    leftContainer->setFixedWidth(220);

    mainLayout->addWidget(leftContainer);
    mainLayout->addWidget(mapWidget, 1);
    mainLayout->addWidget(rightPanelStack);
}

void EditorWindow::setupRightPanel() {
    rightPanelStack = new QStackedWidget();
    rightPanelStack->setFixedWidth(280);
    
    emptyPanel = new QWidget();
    rightPanelStack->addWidget(emptyPanel);

    // --- 节点属性页 ---
    nodePropPanel = new QWidget();
    QVBoxLayout* npLayout = new QVBoxLayout(nodePropPanel);
    QGroupBox* npBox = new QGroupBox("节点属性");
    QVBoxLayout* form = new QVBoxLayout(npBox);
    
    nodeCoordLabel = new QLabel("坐标: -"); form->addWidget(nodeCoordLabel);
    form->addWidget(new QLabel("名称:"));
    nodeNameEdit = new QLineEdit(); form->addWidget(nodeNameEdit);
    form->addWidget(new QLabel("海拔 (Z):"));
    nodeZEdit = new QLineEdit(); form->addWidget(nodeZEdit);
    form->addWidget(new QLabel("分类:"));
    nodeCatCombo = new QComboBox();
    // 补全所有分类
    nodeCatCombo->addItems({
        "None", "Dorm", "Canteen", "Service", "Square", 
        "Gate", "Road", "Park", "Shop", "Playground", 
        "Landmark", "Lake", "Building", "Classroom", 
        "Hotel", "BusStation"
    });
    form->addWidget(nodeCatCombo);
    form->addWidget(new QLabel("描述:"));
    nodeDescEdit = new QLineEdit(); form->addWidget(nodeDescEdit);
    
    QPushButton* applyBtn = new QPushButton("应用修改");
    connect(applyBtn, &QPushButton::clicked, this, &EditorWindow::onSaveNodeProp);
    form->addWidget(applyBtn);
    
    QPushButton* delBtn = new QPushButton("删除节点");
    delBtn->setStyleSheet("color: red;");
    connect(delBtn, &QPushButton::clicked, this, &EditorWindow::onDeleteNode);
    form->addWidget(delBtn);
    
    form->addStretch();
    npLayout->addWidget(npBox);
    rightPanelStack->addWidget(nodePropPanel);

    // --- 边属性页 (这里添加了复选框) ---
    edgePropPanel = new QWidget();
    QVBoxLayout* epLayout = new QVBoxLayout(edgePropPanel);
    QGroupBox* epBox = new QGroupBox("路径连接");
    QVBoxLayout* eform = new QVBoxLayout(epBox);
    
    edgeInfoLabel = new QLabel("-"); eform->addWidget(edgeInfoLabel);
    eform->addWidget(new QLabel("道路类型:"));
    edgeTypeCombo = new QComboBox();
    edgeTypeCombo->addItems({"普通道路 (Normal)", "主干道 (Main)", "小径 (Path)", "室内 (Indoor)", "楼梯 (Stairs)"});
    eform->addWidget(edgeTypeCombo);
    edgeConnectBtn = new QPushButton("建立连接");
    connect(edgeConnectBtn, &QPushButton::clicked, this, &EditorWindow::onConnectEdge);
    eform->addWidget(edgeConnectBtn);
    
    eform->addWidget(new QLabel("路名:"));
    edgeNameEdit = new QLineEdit(); eform->addWidget(edgeNameEdit);
    eform->addWidget(new QLabel("描述:"));
    edgeDescEdit = new QLineEdit(); eform->addWidget(edgeDescEdit);

    // 【新增】坡度复选框
    edgeSlopeCheck = new QCheckBox("是上坡 (A->B)");
    edgeSlopeCheck->setToolTip("勾选表示 A到B 为上坡 (8%)，反向为下坡");
    eform->addWidget(edgeSlopeCheck);
    
    edgeDisconnectBtn = new QPushButton("断开连接");
    edgeDisconnectBtn->setStyleSheet("color: red;");
    connect(edgeDisconnectBtn, &QPushButton::clicked, this, &EditorWindow::onDisconnectEdge);
    eform->addWidget(edgeDisconnectBtn);
    
    eform->addStretch();
    epLayout->addWidget(epBox);
    rightPanelStack->addWidget(edgePropPanel);
}

void EditorWindow::refreshMap() {
    mapWidget->drawMap(model->getAllNodes(), model->getAllEdges());
}

// --- 逻辑槽函数 ---

void EditorWindow::onModeChanged(int id) {
    // 切换模式前，清除旧的激活状态
    mapWidget->setActiveEdge(-1, -1);
    currentNodeId = -1; // 忘记之前的选择
    EditMode m = EditMode::None;
    if (id == 1) m = EditMode::ConnectEdge;
    else if (id == 2) m = EditMode::AddBuilding;
    else if (id == 3) m = EditMode::AddGhost;
    
    mapWidget->setEditMode(m);
    rightPanelStack->setCurrentWidget(emptyPanel);
    statusLabel->setText("模式切换");
}

void EditorWindow::onNodeEditClicked(int nodeId, bool) {
    // 【新增】点击新节点时，清除之前可能存在的连线高亮
    mapWidget->setActiveEdge(-1, -1);
    
    showNodeProperty(nodeId);
}

void EditorWindow::onEmptySpaceClicked(double x, double y) {
    // 【关键修复 1】新建前，先清除任何可能的选中状态
    mapWidget->setActiveEdge(-1, -1);
    
    EditMode m = mapWidget->getEditMode();
    
    if (m == EditMode::AddBuilding) {
        int id = model->addNode(x, y, NodeType::Visible);
        refreshMap();
        showNodeProperty(id);
        statusLabel->setText("新建建筑成功");
    } else if (m == EditMode::AddGhost) {
        // 【关键修复 2】新建路口逻辑
        int id = model->addNode(x, y, NodeType::Ghost);
        
        // 强制刷新一次地图，确保新节点被渲染出来
        refreshMap();
        
        // 然后再显示属性
        showNodeProperty(id);
        
        statusLabel->setText("新建路口成功");
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
    
    // 1. 填充文本框
    nodeNameEdit->setText(n.name);
    nodeDescEdit->setText(n.description);
    nodeZEdit->setText(QString::number(n.z));
    nodeCoordLabel->setText(QString("(%1, %2)").arg((int)n.x).arg((int)n.y));
    
    // 2. 【核心修改】分类逻辑锁定
    if (n.type == NodeType::Ghost) {
        // 如果是 Ghost 节点：
        // 强制选中 "Road"
        int roadIdx = nodeCatCombo->findText("Road");
        if (roadIdx != -1) nodeCatCombo->setCurrentIndex(roadIdx);
        
        // 锁定下拉框，变灰，不允许修改
        nodeCatCombo->setEnabled(false);
        nodeCatCombo->setToolTip("Ghost (路口) 节点的分类固定为 Road");
    } else {
        // 如果是 Visible 建筑：
        // 恢复启用
        nodeCatCombo->setEnabled(true);
        nodeCatCombo->setToolTip("");
        
        // 正常显示当前的分类
        QString catStr = Node::categoryToString(n.category);
        int idx = nodeCatCombo->findText(catStr);
        if (idx != -1) nodeCatCombo->setCurrentIndex(idx);
        else nodeCatCombo->setCurrentIndex(0);
    }
    
    rightPanelStack->setCurrentWidget(nodePropPanel);
}

void EditorWindow::onSaveNodeProp() {
    if (currentNodeId == -1) return;
    
    Node n = model->getNode(currentNodeId);
    
    // 更新基础属性
    n.name = nodeNameEdit->text();
    n.description = nodeDescEdit->text();
    n.z = nodeZEdit->text().toDouble();
    
    // 【修复】获取下拉框选中的文本，并转换回 Enum 存入节点
    QString selectedCat = nodeCatCombo->currentText();
    n.category = Node::stringToCategory(selectedCat);
    
    // 更新模型
    model->updateNode(n);
    
    // 刷新地图（虽然分类可能不影响外观，但为了保险）
    refreshMap();
    
    statusLabel->setText("属性保存成功: " + selectedCat);
}

void EditorWindow::onDeleteNode() {
    if (currentNodeId != -1) {
        mapWidget->setActiveEdge(-1, -1); // 清除高亮
        model->deleteNode(currentNodeId);
        currentNodeId = -1;
        refreshMap();
        rightPanelStack->setCurrentWidget(emptyPanel);
    }
}

// 连边相关
void EditorWindow::onEdgeConnectionRequested(int idA, int idB) {
    currentEdgeU = idA;
    currentEdgeV = idB;
    showEdgePanel(idA, idB);
}

void EditorWindow::showEdgePanel(int u, int v) {
    // 告诉地图：请高亮这两个点和它们之间的连线
    mapWidget->setActiveEdge(u, v);
    
    rightPanelStack->setCurrentWidget(edgePropPanel);
    edgeInfoLabel->setText(QString("%1 <-> %2").arg(u).arg(v));
    
    const Edge* e = model->findEdge(u, v);
    if (e) {
        edgeConnectBtn->setText("更新连接");
        edgeDisconnectBtn->setEnabled(true);
        edgeNameEdit->setText(e->name);
        edgeDescEdit->setText(e->description);
        
        // 【核心修改】根据 slope 的值决定是否勾选
        // 只要 slope 绝对值 > 0.01 就认为是坡
        edgeSlopeCheck->setChecked(std::abs(e->slope) > 0.01);

        edgeTypeCombo->setCurrentIndex(static_cast<int>(e->type));
        
    } else {
        edgeConnectBtn->setText("新建连接");
        edgeDisconnectBtn->setEnabled(false);
        edgeNameEdit->setText("路");
        edgeDescEdit->clear();
        
        // 【核心修改】新建默认无坡
        edgeSlopeCheck->setChecked(false);

        edgeTypeCombo->setCurrentIndex(0); // 默认 Normal
    }
}

void EditorWindow::onConnectEdge() {
    if (currentEdgeU == -1) return;
    Node a = model->getNode(currentEdgeU);
    Node b = model->getNode(currentEdgeV);
    
    // 1. 计算距离 (像素 -> 米，应用 0.91 比例尺)
    double pixelDist = std::hypot(a.x - b.x, a.y - b.y);
    double realDist = pixelDist * 0.91;

    Edge e;
    e.u = currentEdgeU; 
    e.v = currentEdgeV;
    e.distance = realDist;
    
    // 2. 【新增】从下拉框读取道路类型 (不再是写死的 Normal)
    e.type = static_cast<EdgeType>(edgeTypeCombo->currentIndex());
    
    // 3. 【新增】从复选框读取坡度 (勾选=0.08, 没勾=0.0)
    e.slope = edgeSlopeCheck->isChecked() ? 0.08 : 0.0; 
    
    e.name = edgeNameEdit->text();
    e.description = edgeDescEdit->text();
    
    model->addOrUpdateEdge(e);
    refreshMap();
    showEdgePanel(currentEdgeU, currentEdgeV);
}

void EditorWindow::onDisconnectEdge() {
    if (currentEdgeU != -1) {
        model->deleteEdge(currentEdgeU, currentEdgeV);
        refreshMap();
        showEdgePanel(currentEdgeU, currentEdgeV);
    }
}

void EditorWindow::onSaveFile() {
    QString appDir = QCoreApplication::applicationDirPath();
    if (model->saveData(appDir + "/Data/nodes.txt", appDir + "/Data/edges.txt")) {
        QMessageBox::information(this, "保存", "地图数据已保存！\n(主窗口可能需要重启加载)");
        emit dataChanged();
    } else {
        QMessageBox::critical(this, "错误", "保存失败");
    }
}