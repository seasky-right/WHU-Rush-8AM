#ifndef VIEW_MAINWINDOW_H
#define VIEW_MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include "../model/GraphModel.h"
#include "MapWidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onMapNodeClicked(int nodeId, QString name, bool isLeftClick);
    void onStartSearch();

private:
    GraphModel* model;
    MapWidget* mapWidget;

    QLineEdit* startEdit;
    QLineEdit* endEdit;
    QPushButton* searchBtn;
    QLabel* statusLabel;

    int currentStartId = -1;
    int currentEndId = -1;

    void setupUi();
};
#endif // VIEW_MAINWINDOW_H
