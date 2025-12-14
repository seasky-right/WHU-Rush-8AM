#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QWheelEvent> // 必须引入
#include "GraphData.h"

class MapWidget : public QGraphicsView
{
    Q_OBJECT
public:
    explicit MapWidget(QWidget *parent = nullptr);

    void drawMap(const QVector<Node>& nodes, const QVector<Edge>& edges);
    void setStartNode(int nodeId);
    void setEndNode(int nodeId);

signals:
    void nodeClicked(int nodeId, QString name, bool isLeftClick);

protected:
    void mousePressEvent(QMouseEvent *event) override;

    // [新增] 滚轮缩放事件
    void wheelEvent(QWheelEvent *event) override;

private:
    QGraphicsScene* scene;
    QVector<Node> cachedNodes;
    int findNodeAt(const QPointF& pos);
};

#endif // MAPWIDGET_H
