#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QGraphicsPixmapItem>
#include <QPixmap>
#include "../GraphData.h"

class MapWidget : public QGraphicsView
{
    Q_OBJECT
public:
    explicit MapWidget(QWidget *parent = nullptr);

    void drawMap(const QVector<Node>& nodes, const QVector<Edge>& edges);
    void setBackgroundImage(const QString& path);
    void setStartNode(int nodeId);
    void setEndNode(int nodeId);

signals:
    void nodeClicked(int nodeId, QString name, bool isLeftClick);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QGraphicsScene* scene;
    QVector<Node> cachedNodes;
    int findNodeAt(const QPointF& pos);
    QGraphicsPixmapItem* backgroundItem = nullptr;
    double currentScale = 1.0;  // 当前缩放因子
    
    // 鼠标拖动状态
    bool isDragging = false;
    QPoint lastMousePos;
};

#endif // MAPWIDGET_H
