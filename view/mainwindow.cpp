#include "MainWindow.h"
#include <QtWidgets/QMessageBox>
#include <QtCore/QCoreApplication>
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    model = new GraphModel();
    mapWidget = new MapWidget(this);

    setupUi();

    // 信号连接
    connect(mapWidget, &MapWidget::nodeEditClicked, this, &MainWindow::onNodeClicked);
    connect(mapWidget, &MapWidget::emptySpaceClicked, this, &MainWindow::onEmptySpaceClicked);
    connect(mapWidget, &MapWidget::edgeConnectionRequested, this, &MainWindow::onEdgeConnectionRequested);
    connect(mapWidget, &MapWidget::nodeMoved, this, &MainWindow::onNodeMoved);
    connect(mapWidget, &MapWidget::undoRequested, this, &MainWindow::onUndoRequested);

    // 加载数据
    QString appDir = QCoreApplication::applicationDirPath();
    if (model->loadData(appDir + "/Data/nodes.txt", appDir + "/Data/edges.txt")) {
        refreshMap();
        mapWidget->setBackgroundImage(appDir + "/Data/map.png");
    }
}

MainWindow::~MainWindow() { delete model; }

void MainWindow::setupUi() {
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QHBoxLayout* mainLayout = new QHBoxLayout(central);

    // --- 左侧栏 ---
    QVBoxLayout* leftLayout = new QVBoxLayout();
    QGroupBox* modeBox = new QGroupBox("编辑器模式");
    QVBoxLayout* modeLayout = new QVBoxLayout(modeBox);
    
    modeGroup = new QButtonGroup(this);
    QRadioButton* rbView = new QRadioButton("浏览模式");
    QRadioButton* rbEdge = new QRadioButton("连边模式");
    QRadioButton* rbBuild = new QRadioButton("新建筑物");
    QRadioButton* rbGhost = new QRadioButton("新幽灵节点");
    
    modeGroup->addButton(rbView, 0);
    modeGroup->addButton(rbEdge, 1);
    modeGroup->addButton(rbBuild, 2);
    modeGroup->addButton(rbGhost, 3);
    rbView->setChecked(true);
    
    modeLayout->addWidget(rbView);
    modeLayout->addWidget(rbEdge);
    modeLayout->addWidget(rbBuild);
    modeLayout->addWidget(rbGhost);
    
    connect(modeGroup, &QButtonGroup::idClicked, this, &MainWindow::onModeChanged);
    
    leftLayout->addWidget(modeBox);
    leftLayout->addStretch();
    
    saveAllBtn = new QPushButton("💾 保存所有修改");
    saveAllBtn->setStyleSheet("background-color: #e74c3c; color: white; font-weight: bold; padding: 10px;");
    connect(saveAllBtn, &QPushButton::clicked, this, &MainWindow::onSaveAll);
    leftLayout->addWidget(saveAllBtn);

    statusLabel = new QLabel("就绪 (中键拖动地图)");
    leftLayout->addWidget(statusLabel);

    // --- 右侧栏 (Stacked) ---
    setupRightPanel();

    // 布局组合
    QWidget* leftContainer = new QWidget();
    leftContainer->setLayout(leftLayout);
    leftContainer->setFixedWidth(200);

    mainLayout->addWidget(leftContainer);
    mainLayout->addWidget(mapWidget, 1);
    mainLayout->addWidget(rightPanelStack);
}

void MainWindow::setupRightPanel() {
    rightPanelStack = new QStackedWidget();
    rightPanelStack->setFixedWidth(250);
    
    // 0. 空白页
    emptyPanel = new QWidget();
    rightPanelStack->addWidget(emptyPanel);

    // 1. 节点属性页
    nodePropPanel = new QWidget();
    QVBoxLayout* npLayout = new QVBoxLayout(nodePropPanel);
    QGroupBox* npBox = new QGroupBox("节点属性");
    QVBoxLayout* form = new QVBoxLayout(npBox);
    
    nodeCoordLabel = new QLabel("坐标: -");
    form->addWidget(nodeCoordLabel);
    
    form->addWidget(new QLabel("名称:"));
    nodeNameEdit = new QLineEdit();
    form->addWidget(nodeNameEdit);
    
    form->addWidget(new QLabel("海拔 (Z):"));
    nodeZEdit = new QLineEdit();
    form->addWidget(nodeZEdit);
    
    form->addWidget(new QLabel("分类:"));
    nodeCatCombo = new QComboBox();
    // 填充 Enum Category
    nodeCatCombo->addItems({"None", "Dorm", "Canteen", "Classroom", "Road"}); // 简略
    form->addWidget(nodeCatCombo);
    
    form->addWidget(new QLabel("描述:"));
    nodeDescEdit = new QLineEdit();
    form->addWidget(nodeDescEdit);
    
    nodeSaveBtn = new QPushButton("应用修改");
    connect(nodeSaveBtn, &QPushButton::clicked, this, &MainWindow::onSaveNodeProp);
    form->addWidget(nodeSaveBtn);
    
    nodeDeleteBtn = new QPushButton("删除此节点");
    nodeDeleteBtn->setStyleSheet("color: red;");
    connect(nodeDeleteBtn, &QPushButton::clicked, this, &MainWindow::onDeleteNode);
    form->addWidget(nodeDeleteBtn);
    
    form->addStretch();
    npLayout->addWidget(npBox);
    rightPanelStack->addWidget(nodePropPanel);

    // 2. 边属性页
    edgePropPanel = new QWidget();
    QVBoxLayout* epLayout = new QVBoxLayout(edgePropPanel);
    QGroupBox* epBox = new QGroupBox("连接管理");
    QVBoxLayout* eform = new QVBoxLayout(epBox);
    
    edgeInfoLabel = new QLabel("选择两点以连接");
    eform->addWidget(edgeInfoLabel);
    
    edgeConnectBtn = new QPushButton("🔗 建立连接");
    connect(edgeConnectBtn, &QPushButton::clicked, this, &MainWindow::onConnectEdge);
    eform->addWidget(edgeConnectBtn);
    
    eform->addWidget(new QLabel("道路名称:"));
    edgeNameEdit = new QLineEdit();
    eform->addWidget(edgeNameEdit);
    
    eform->addWidget(new QLabel("描述:"));
    edgeDescEdit = new QLineEdit();
    eform->addWidget(edgeDescEdit);
    
    edgeDisconnectBtn = new QPushButton("💔 断开连接");
    edgeDisconnectBtn->setStyleSheet("color: red;");
    connect(edgeDisconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnectEdge);
    eform->addWidget(edgeDisconnectBtn);
    
    eform->addStretch();
    epLayout->addWidget(epBox);
    rightPanelStack->addWidget(edgePropPanel);
}

void MainWindow::refreshMap() {
    mapWidget->drawMap(model->getAllNodes(), model->getAllEdges());
}

// --- 逻辑处理 ---

void MainWindow::onModeChanged(int id) {
    EditMode m = EditMode::None;
    if (id == 1) m = EditMode::ConnectEdge;
    else if (id == 2) m = EditMode::AddBuilding;
    else if (id == 3) m = EditMode::AddGhost;
    
    mapWidget->setEditMode(m);
    rightPanelStack->setCurrentWidget(emptyPanel);
    statusLabel->setText(QString("模式切换: %1").arg(modeGroup->button(id)->text()));
}

void MainWindow::onNodeClicked(int nodeId, bool isCtrlPressed) {
    // 只有在非浏览模式下，或者浏览模式也可以看属性
    // 需求说：Building/Ghost模式下点选已有节点修改
    EditMode m = mapWidget->getEditMode();
    if (m == EditMode::AddBuilding || m == EditMode::AddGhost || m == EditMode::None) {
        showNodeProperty(nodeId);
    }
}

void MainWindow::onEmptySpaceClicked(double x, double y) {
    EditMode m = mapWidget->getEditMode();
    if (m == EditMode::AddBuilding) {
        // 直接创建新建筑
        int newId = model->addNode(x, y, NodeType::Visible);
        refreshMap();
        showNodeProperty(newId);
        statusLabel->setText("新建建筑物成功");
    } else if (m == EditMode::AddGhost) {
        // 直接创建路口
        int newId = model->addNode(x, y, NodeType::Ghost);
        refreshMap();
        showNodeProperty(newId);
        statusLabel->setText("新建路口成功");
    }
}

void MainWindow::onNodeMoved(int id, double x, double y) {
    Node n = model->getNode(id);
    n.x = x; n.y = y;
    model->updateNode(n);
    refreshMap(); // 刷新显示
    // 如果当前正开着属性面板，更新坐标显示
    if (currentNodeId == id && rightPanelStack->currentWidget() == nodePropPanel) {
        nodeCoordLabel->setText(QString("坐标: (%1, %2)").arg(x, 0, 'f', 1).arg(y, 0, 'f', 1));
    }
}

void MainWindow::onUndoRequested() {
    if (model->canUndo()) {
        model->undo();
        refreshMap();
        statusLabel->setText("已撤销上一步操作");
        rightPanelStack->setCurrentWidget(emptyPanel);
    } else {
        statusLabel->setText("无可撤销的操作");
    }
}

void MainWindow::showNodeProperty(int id) {
    currentNodeId = id;
    Node n = model->getNode(id);
    
    nodeNameEdit->setText(n.name);
    nodeDescEdit->setText(n.description);
    nodeZEdit->setText(QString::number(n.z));
    nodeCoordLabel->setText(QString("坐标: (%1, %2)").arg(n.x, 0, 'f', 1).arg(n.y, 0, 'f', 1));
    
    // 阻止某些修改：x,y 不可改(已通过Label实现)，其他可改
    // 切换面板
    rightPanelStack->setCurrentWidget(nodePropPanel);
}

void MainWindow::onSaveNodeProp() {
    if (currentNodeId == -1) return;
    Node n = model->getNode(currentNodeId);
    n.name = nodeNameEdit->text();
    n.description = nodeDescEdit->text();
    n.z = nodeZEdit->text().toDouble();
    // Category 略
    
    model->updateNode(n);
    refreshMap();
    statusLabel->setText("节点属性已更新");
}

void MainWindow::onDeleteNode() {
    if (currentNodeId == -1) return;
    model->deleteNode(currentNodeId);
    currentNodeId = -1;
    refreshMap();
    rightPanelStack->setCurrentWidget(emptyPanel);
    statusLabel->setText("节点已删除");
}

// --- 连边逻辑 ---

void MainWindow::onEdgeConnectionRequested(int idA, int idB) {
    currentEdgeU = idA;
    currentEdgeV = idB;
    showEdgePanel(idA, idB);
}

void MainWindow::showEdgePanel(int u, int v) {
    rightPanelStack->setCurrentWidget(edgePropPanel);
    edgeInfoLabel->setText(QString("连接: %1 <-> %2").arg(u).arg(v));
    
    const Edge* e = model->findEdge(u, v);
    if (e) {
        edgeConnectBtn->setText("更新连接数据");
        edgeDisconnectBtn->setEnabled(true);
        edgeNameEdit->setText(e->name);
        edgeDescEdit->setText(e->description);
    } else {
        edgeConnectBtn->setText("建立新连接");
        edgeDisconnectBtn->setEnabled(false);
        edgeNameEdit->setText("自动道路");
        edgeDescEdit->clear();
    }
}

void MainWindow::onConnectEdge() {
    if (currentEdgeU == -1 || currentEdgeV == -1) return;
    
    Node a = model->getNode(currentEdgeU);
    Node b = model->getNode(currentEdgeV);
    double dist = std::hypot(a.x - b.x, a.y - b.y);
    
    Edge e;
    e.u = currentEdgeU; e.v = currentEdgeV;
    e.distance = dist; // 自动计算
    e.type = EdgeType::Normal;
    e.isSlope = false; 
    e.name = edgeNameEdit->text();
    e.description = edgeDescEdit->text();
    
    model->addOrUpdateEdge(e);
    refreshMap();
    showEdgePanel(currentEdgeU, currentEdgeV); // 刷新界面状态
    statusLabel->setText("连接已建立/更新");
}

void MainWindow::onDisconnectEdge() {
    if (currentEdgeU == -1 || currentEdgeV == -1) return;
    model->deleteEdge(currentEdgeU, currentEdgeV);
    refreshMap();
    showEdgePanel(currentEdgeU, currentEdgeV);
    statusLabel->setText("连接已断开");
}

void MainWindow::onSaveAll() {
    QString appDir = QCoreApplication::applicationDirPath();
    bool ok = model->saveData(appDir + "/Data/nodes.txt", appDir + "/Data/edges.txt");
    if (ok) QMessageBox::information(this, "保存", "数据已成功保存至文件！");
    else QMessageBox::critical(this, "错误", "保存失败，请检查文件权限！");
}

