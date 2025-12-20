#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include "../model/GraphModel.h"
#include "MapWidget.h"

class EditorWindow : public QMainWindow
{
    Q_OBJECT
public:
    // 接收共享的 Model 指针，确保修改的是同一份数据
    explicit EditorWindow(GraphModel* sharedModel, QWidget *parent = nullptr);

signals:
    void dataChanged(); // 通知主窗口刷新

private slots:
    // --- 地图交互信号 ---
    void onNodeEditClicked(int nodeId, bool isCtrlPressed);
    void onEmptySpaceClicked(double x, double y);
    void onEdgeConnectionRequested(int idA, int idB);
    void onNodeMoved(int id, double x, double y);
    void onUndoRequested();

    // --- 模式与功能 ---
    void onModeChanged(int id);
    void onDeleteNode();
    void onDisconnectEdge();
    void onSaveFile();

    // --- 【新增】实时属性响应 (替代旧的手动保存槽) ---
    void onLiveNodePropChanged();
    void onLiveEdgePropChanged();

private:
    GraphModel* model;
    MapWidget* mapWidget;

    QButtonGroup* modeGroup;
    QLabel* statusLabel;

    QStackedWidget* rightPanelStack;
    QWidget* emptyPanel;
    QWidget* nodePropPanel;
    QWidget* edgePropPanel;

    // --- 节点属性控件 ---
    QLineEdit *nodeNameEdit;
    QLineEdit *nodeDescEdit;
    QLineEdit *nodeZEdit; // 海拔输入
    QLabel *nodeCoordLabel;
    QComboBox *nodeCatCombo;
    int currentNodeId = -1;

    // --- 边属性控件 ---
    QLabel *edgeInfoLabel;
    QLineEdit *edgeNameEdit;
    QLineEdit *edgeDescEdit;
    QPushButton *edgeDisconnectBtn; // 仅保留断开按钮
    QCheckBox* edgeSlopeCheck;      // 控制坡度
    QComboBox *edgeTypeCombo;       // 道路类型
    int currentEdgeU = -1, currentEdgeV = -1;

    // --- 内部辅助函数 ---
    void setupUi();
    void setupRightPanel();
    void refreshMap();
    void showNodeProperty(int id);
    void showEdgePanel(int u, int v);
};