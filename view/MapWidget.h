#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QGraphicsPixmapItem>
#include <QGraphicsLineItem>
#include <QPixmap>
#include <QTimer>
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
    
    // 路径高亮和动画接口
    void highlightPath(const QVector<int>& pathNodeIds, double animationDuration = 1.0);

signals:
    void nodeClicked(int nodeId, QString name, bool isLeftClick);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onAnimationTick();

private:
    QGraphicsScene* scene;
    QVector<Node> cachedNodes;
    int findNodeAt(const QPointF& pos);
    QGraphicsPixmapItem* backgroundItem = nullptr;
    double currentScale = 1.0;  // 当前缩放因子
    
    // 鼠标拖动状态
    bool isDragging = false;
    QPoint lastMousePos;
    
    // 路径动画相关
    QVector<int> currentPathNodeIds;
    QGraphicsLineItem* pathGrowthLine = nullptr;  // 生长中的路径线
    QTimer* animationTimer = nullptr;
    double animationProgress = 0.0;  // 0 到 1.0
    double animationDurationMs = 1000.0;  // 动画持续时间（毫秒）
    qint64 animationStartTime = 0;
    
    // 清除路径高亮和动画
    void clearPathHighlight();
    void drawPathGrowthAnimation();
};

#endif // MAPWIDGET_H
