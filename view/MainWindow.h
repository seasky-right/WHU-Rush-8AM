#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtCore/QVector>
#include <QtCore/QString>
#include "../model/GraphModel.h"
#include "../model/PathRecommendation.h"
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

private:
    GraphModel* model;
    MapWidget* mapWidget;

    QLineEdit* startEdit;
    QLineEdit* endEdit;
    QPushButton* searchBtn;
    QCheckBox* editModeCheck = nullptr;
    QPushButton* saveNodeBtn = nullptr;
    QLineEdit* nodeNameEdit = nullptr;
    QComboBox* nodeTypeCombo = nullptr;
    QLineEdit* nodeDescEdit = nullptr;
    QLineEdit* nodeCategoryEdit = nullptr;
    QCheckBox* connectPrevCheck = nullptr;
    QLineEdit* edgeDescEdit = nullptr;
    QLabel* nodeCoordLabel = nullptr;
    QLabel* statusLabel;

    // 路线推荐相关
    QWidget* routePanelWidget;
    QScrollArea* routeScrollArea;
    QVBoxLayout* routePanelLayout;
    QVector<RouteButton*> routeButtons;
    QVector<PathRecommendation> currentRecommendations;

    int currentStartId = -1;
    int currentEndId = -1;
    int lastSavedNodeId = -1;

    void setupUi();
    void displayRouteRecommendations(const QVector<PathRecommendation>& recommendations);
    void clearRoutePanel();
    void setupEditorPanel(QVBoxLayout* panelLayout);
    void onEditPointPicked(QPointF pos, bool ctrlPressed);
    void onSaveNode();
};
 
