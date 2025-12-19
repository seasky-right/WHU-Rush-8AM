#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QCheckBox>
#include <QtCore/QVector>
#include "../model/GraphModel.h"
#include "MapWidget.h"
#include "RouteButton.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onMapNodeClicked(int nodeId, QString name, bool isLeftClick);
    void onStartSearch();
    void onRouteButtonClicked(int routeIndex);
    void onRouteHovered(const PathRecommendation& recommendation);
    void onRouteUnhovered();
    
    // 打开编辑器
    void onOpenEditor();
    // 编辑器保存后刷新
    void onMapDataChanged();

private:
    GraphModel* model;
    MapWidget* mapWidget;

    // 导航控件
    QLineEdit* startEdit;
    QLineEdit* endEdit;
    QPushButton* searchBtn;
    QLabel* statusLabel;
    QPushButton* openEditorBtn; // 新增入口

    // 路线推荐 UI
    QWidget* routePanelWidget;
    QScrollArea* routeScrollArea;
    QVBoxLayout* routePanelLayout;
    QVector<RouteButton*> routeButtons;
    QVector<PathRecommendation> currentRecommendations;

    int currentStartId = -1;
    int currentEndId = -1;

    void setupUi();
    void displayRouteRecommendations(const QVector<PathRecommendation>& recommendations);
    void clearRoutePanel();
};