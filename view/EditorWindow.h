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
    void onNodeEditClicked(int nodeId, bool isCtrlPressed);
    void onEmptySpaceClicked(double x, double y);
    void onEdgeConnectionRequested(int idA, int idB);
    void onNodeMoved(int id, double x, double y);
    void onUndoRequested();

    void onModeChanged(int id);
    void onDeleteNode();
    void onSaveNodeProp();
    void onConnectEdge();
    void onDisconnectEdge();
    void onSaveFile();

private:
    GraphModel* model;
    MapWidget* mapWidget;

    QButtonGroup* modeGroup;
    QLabel* statusLabel;

    QStackedWidget* rightPanelStack;
    QWidget* emptyPanel;
    QWidget* nodePropPanel;
    QWidget* edgePropPanel;

    // 节点属性控件
    QLineEdit *nodeNameEdit, *nodeDescEdit, *nodeZEdit;
    QLabel *nodeCoordLabel;
    QComboBox *nodeCatCombo;
    int currentNodeId = -1;

    // 边属性控件
    QLabel *edgeInfoLabel;
    QLineEdit *edgeNameEdit, *edgeDescEdit;
    QPushButton *edgeConnectBtn, *edgeDisconnectBtn;
    int currentEdgeU = -1, currentEdgeV = -1;

    void setupUi();
    void setupRightPanel();
    void refreshMap();
    void showNodeProperty(int id);
    void showEdgePanel(int u, int v);
};