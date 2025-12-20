#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout> // [新增]
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QTimeEdit>
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
    
    // [修改] 统一的模式搜索槽函数
    void onModeSearch(TransportMode mode);
    
    void onRouteButtonClicked(int routeIndex);
    void onRouteHovered(const PathRecommendation& recommendation);
    void onRouteUnhovered();
    void onOpenEditor();
    void onMapDataChanged();

private:
    GraphModel* model;
    MapWidget* mapWidget;

    QLineEdit* startEdit;
    QLineEdit* endEdit;
    
    QComboBox* weatherCombo;
    QTimeEdit* timeCurrentEdit;
    QTimeEdit* timeClassEdit;

    // [修改] 分开的交通工具按钮
    QPushButton* btnWalk;
    QPushButton* btnBike;
    QPushButton* btnEBike;
    QPushButton* btnRun;
    QPushButton* btnBus;

    QLabel* statusLabel;
    QPushButton* openEditorBtn;

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