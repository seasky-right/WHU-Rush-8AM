#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QStackedWidget>
#include "../model/GraphModel.h"
#include "MapWidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 地图信号槽
    void onNodeClicked(int nodeId, bool isCtrlPressed);
    void onEmptySpaceClicked(double x, double y);
    void onEdgeConnectionRequested(int idA, int idB);
    void onNodeMoved(int id, double x, double y);
    void onUndoRequested();

    // UI 按钮槽
    void onModeChanged(int id); // 0=None, 1=Connect, 2=Building, 3=Ghost
    void onDeleteNode();
    void onSaveNodeProp(); // 修改属性后保存
    void onConnectEdge();  // 确认连接
    void onDisconnectEdge(); // 断开
    void onSaveAll();      // 保存到文件

private:
    GraphModel* model;
    MapWidget* mapWidget;

    // --- 左侧控制 ---
    QButtonGroup* modeGroup;
    QPushButton* saveAllBtn;
    QLabel* statusLabel;

    // --- 右侧面板 (Stacked Widget) ---
    QStackedWidget* rightPanelStack;
    QWidget* emptyPanel;
    
    // 面板1: 节点属性 (用于新建/修改 建筑或幽灵)
    QWidget* nodePropPanel;
    QLineEdit* nodeNameEdit;
    QLineEdit* nodeDescEdit;
    QLineEdit* nodeZEdit;
    QLabel* nodeCoordLabel;
    QComboBox* nodeCatCombo;
    QPushButton* nodeDeleteBtn;
    QPushButton* nodeSaveBtn;
    int currentNodeId = -1; // 当前编辑的节点

    // 面板2: 边属性 (连边模式)
    QWidget* edgePropPanel;
    QLabel* edgeInfoLabel; // 显示 A -> B
    QLineEdit* edgeNameEdit;
    QLineEdit* edgeDescEdit;
    QPushButton* edgeConnectBtn;
    QPushButton* edgeDisconnectBtn;
    int currentEdgeU = -1;
    int currentEdgeV = -1;

    void setupUi();
    void refreshMap();
    void setupRightPanel();
    
    // 辅助
    void showNodeProperty(int id);
    void showNewNodeDialog(double x, double y, NodeType type);
    void showEdgePanel(int u, int v);
};