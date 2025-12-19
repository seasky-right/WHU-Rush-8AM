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
    // 加载背景
    QString appDir = QCoreApplication::applicationDirPath();
    mapWidget->setBackgroundImage(appDir + "/Data/map.png");
    
    // 初始化 UI
    setupUi();

    // 信号连接
    connect(mapWidget, &MapWidget::nodeEditClicked, this, &EditorWindow::onNodeEditClicked);
    connect(mapWidget, &MapWidget::emptySpaceClicked, this, &EditorWindow::onEmptySpaceClicked);
    connect(mapWidget, &MapWidget::edgeConnectionRequested, this, &EditorWindow::onEdgeConnectionRequested);
    connect(mapWidget, &MapWidget::nodeMoved, this, &EditorWindow::onNodeMoved);
    connect(mapWidget, &MapWidget::undoRequested, this, &EditorWindow::onUndoRequested);

    // 初始刷新
    refreshMap();
    
    // 默认进入浏览模式，防止误触
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
    nodeCatCombo->addItems({"None", "Dorm", "Canteen", "Classroom", "Road", "Gate", "Playground"});
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

    // --- 边属性页 ---
    edgePropPanel = new QWidget();
    QVBoxLayout* epLayout = new QVBoxLayout(edgePropPanel);
    QGroupBox* epBox = new QGroupBox("路径连接");
    QVBoxLayout* eform = new QVBoxLayout(epBox);
    
    edgeInfoLabel = new QLabel("-"); eform->addWidget(edgeInfoLabel);
    edgeConnectBtn = new QPushButton("建立连接");
    connect(edgeConnectBtn, &QPushButton::clicked, this, &EditorWindow::onConnectEdge);
    eform->addWidget(edgeConnectBtn);
    
    eform->addWidget(new QLabel("路名:"));
    edgeNameEdit = new QLineEdit(); eform->addWidget(edgeNameEdit);
    eform->addWidget(new QLabel("描述:"));
    edgeDescEdit = new QLineEdit(); eform->addWidget(edgeDescEdit);
    
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
    
    // 2. 【修复】正确显示当前的分类
    // 获取节点分类的字符串表示 (例如 "Dorm")
    QString catStr = Node::categoryToString(n.category);
    
    // 在下拉框中查找这个字符串，并设置为选中状态
    int idx = nodeCatCombo->findText(catStr);
    if (idx != -1) {
        nodeCatCombo->setCurrentIndex(idx);
    } else {
        nodeCatCombo->setCurrentIndex(0); // 没找到则默认 None
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
    } else {
        edgeConnectBtn->setText("新建连接");
        edgeDisconnectBtn->setEnabled(false);
        edgeNameEdit->setText("路");
        edgeDescEdit->clear();
    }
}

void EditorWindow::onConnectEdge() {
    if (currentEdgeU == -1) return;
    Node a = model->getNode(currentEdgeU);
    Node b = model->getNode(currentEdgeV);
    double dist = std::hypot(a.x - b.x, a.y - b.y);
    Edge e;
    e.u = currentEdgeU; e.v = currentEdgeV;
    e.distance = dist;
    e.type = EdgeType::Normal;
    e.isSlope = false; 
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