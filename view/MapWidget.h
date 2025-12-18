#pragma once

#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsScene>
#include <QtGui/QWheelEvent>
#include <QtGui/QResizeEvent>
#include <QtWidgets/QGraphicsPixmapItem>
#include <QtWidgets/QGraphicsLineItem>
#include <QtCore/QTimer>
#include <QtCore/QMap>
#include <QtCore/QPointer>
#include <QtCore/QVariantAnimation>
#include "../GraphData.h" // 引入新的 Enums

class MapEditor;

class MapWidget : public QGraphicsView
{
    Q_OBJECT
public:
    explicit MapWidget(QWidget *parent = nullptr);

    void drawMap(const QVector<Node>& nodes, const QVector<Edge>& edges);
    void setBackgroundImage(const QString& path);
    void setStartNode(int nodeId);
    void setEndNode(int nodeId);

    void setEditMode(bool enabled);
    bool isEditMode() const { return editMode; }
    MapEditor* getEditor() const { return mapEditor; }
    
    void clearEditTempItems();
    void addEditVisualNode(int id, const QString& name, const QPointF& pos, int type); // 保持 int 接口方便调用
    
    void highlightPath(const QVector<int>& pathNodeIds, double animationDuration = 1.0);
    void clearPathHighlight();

signals:
    void nodeClicked(int nodeId, QString name, bool isLeftClick);
    void editPointPicked(QPointF scenePos, bool ctrlPressed);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void onAnimationTick();
    void resumeHoverAnimations();

private:
    QGraphicsScene* scene;
    QVector<Node> cachedNodes;
    QVector<Edge> cachedEdges;
    QMap<int, QGraphicsTextItem*> nodeLabelItems;
    
    int findNodeAt(const QPointF& pos);
    int findEdgeAt(const QPointF& pos, QPointF& closestPoint, int& outU, int& outV);
    
    QGraphicsPixmapItem* backgroundItem = nullptr;
    double currentScale = 1.0;
    
    // 悬停相关
    int hoveredNodeId = -1;
    int hoveredEdgeIndex = -1;
    QVector<QGraphicsItem*> hoverItems;
    QVector<int> hiddenLabelNodeIds;
    QVector<QPointer<QAbstractAnimation>> hoverAnims;
    QTimer* hoverResumeTimer = nullptr;
    quint64 hoverUidCounter = 1;
    
    void clearHoverItems();
    void showNodeHoverBubble(const Node& node);
    void showEdgeHoverBubble(const Edge& edge, const QPointF& closestPoint);
    QColor withAlpha(const QColor& c, int alpha);
    void startHoverAppearAnimation();
    void pauseHoverAnimations();
    QColor nodeBaseColor(const Node& node) const;
    QColor edgeBaseColor(const Edge& edge) const;
    
    // 路径动画相关
    QVector<int> currentPathNodeIds;
    double animationProgress = 0.0;
    double animationDurationMs = 1000.0;
    qint64 animationStartTime = 0;
    QTimer* animationTimer = nullptr;
    void drawPathGrowthAnimation();
    
    // 辅助
    QPen edgePenForType(EdgeType type) const; // 更新为 Enum
    QPainterPath buildSmoothPath(const QVector<QPointF>& pts) const;
    QPainterPath buildPartialPathFromLength(const QPainterPath& fullPath, double length) const;

    bool editMode = false;
    MapEditor* mapEditor = nullptr;
    QVector<QGraphicsItem*> editTempItems;
};