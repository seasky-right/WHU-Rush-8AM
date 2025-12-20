#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSpinBox> // 替换 QTimeEdit
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
    void onModeSearch(TransportMode mode);
    void onRouteButtonClicked(int routeIndex);
    void onRouteHovered(const PathRecommendation& recommendation);
    void onRouteUnhovered();
    void onOpenEditor();
    void onMapDataChanged();

private:
    GraphModel* model;
    MapWidget* mapWidget;

    // 左侧控件
    QLineEdit* startEdit;
    QLineEdit* endEdit;
    QComboBox* weatherCombo;
    
    // 【修改】拆分时间选择器为 时/分 SpinBox
    QSpinBox* spinCurrHour;
    QSpinBox* spinCurrMin;
    QSpinBox* spinClassHour;
    QSpinBox* spinClassMin;
    
    QCheckBox* lateCheckToggle;

    // 交通工具按钮
    QPushButton* btnWalk;
    QPushButton* btnBike;
    QPushButton* btnEBike;
    QPushButton* btnRun;
    QPushButton* btnBus;

    QPushButton* openEditorBtn;
    QLabel* statusLabel;

    // 结果面板
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
    void resetAllButtonStyles();
    void updateButtonStyle(QPushButton* btn, bool isSelected, bool isLate);
};