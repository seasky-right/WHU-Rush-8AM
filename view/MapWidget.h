#pragma once

#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsPixmapItem>
#include <QtWidgets/QGraphicsLineItem>
#include <QtWidgets/QGraphicsPathItem> 
#include <QtWidgets/QGraphicsTextItem>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtCore/QTimer>
#include <QtCore/QMap>
#include <QtCore/QPointer>
#include <QtCore/QVariantAnimation>
#include <QtCore/QPropertyAnimation>
#include "../GraphData.h"

enum class EditMode {
    None,           
    ConnectEdge,    
    AddBuilding,    
    AddGhost        
};

class MapWidget : public QGraphicsView
{
    Q_OBJECT
public:
    explicit MapWidget(QWidget *parent = nullptr);

    void drawMap(const QVector<Node>& nodes, const QVector<Edge>& edges);
    void setBackgroundImage(const QString& path);
    void setEditMode(EditMode mode);
    EditMode getEditMode() const { return currentMode; }

    void clearEditTempItems();
    void addEditVisualNode(int id, const QString& name, const QPointF& pos, int type);

    void highlightPath(const QVector<int>& pathNodeIds, double animationDuration = 1.0);
    void clearPathHighlight();
    void setActiveEdge(int u, int v);

signals:
    void nodeClicked(int nodeId, QString name, bool isLeftClick); 
    void nodeEditClicked(int nodeId, bool isCtrlPressed);
    void emptySpaceClicked(double x, double y);
    void edgeConnectionRequested(int idA, int idB);
    void nodeMoved(int id, double newX, double newY);
    void undoRequested(); 

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
    QGraphicsPixmapItem* backgroundItem = nullptr;
    
    QVector<Node> cachedNodes;
    QVector<Edge> cachedEdges;
    QMap<int, QGraphicsTextItem*> nodeLabelItems;
    
    EditMode currentMode = EditMode::None;
    
    bool isMiddlePanning = false;
    QPoint lastPanPos;
    bool isNodeDragging = false;
    int draggingNodeId = -1;
    QPointF lastScenePos;
    
    int connectFirstNodeId = -1;
    QVector<QGraphicsItem*> editTempItems; 

    int activeEdgeU = -1;
    int activeEdgeV = -1;

    // --- 悬停气泡管理 ---
    int hoveredNodeId = -1;
    int hoveredEdgeIndex = -1;
    
    // 【新增】气泡自适应缩放比例
    double currentBubbleScale = 1.0;

    QVector<QGraphicsItem*> hoverItems;
    QVector<QGraphicsItem*> dyingItems; 
    
    QVector<int> hiddenLabelNodeIds;
    QVector<QPointer<QAbstractAnimation>> hoverAnims;
    QTimer* hoverResumeTimer = nullptr;
    quint64 hoverUidCounter = 1;

    void clearHoverItems();
    void fadeOutHoverItems();
    void killDyingItems(); 
    void stopHoverAnimations();
    
    void showNodeHoverBubble(const Node& node);
    void showEdgeHoverBubble(const Edge& edge, const QPointF& closestPoint);
    void startHoverAppearAnimation();
    void pauseHoverAnimations();
    QColor withAlpha(const QColor& c, int alpha);
    
    // --- 路径动画 ---
    QVector<int> currentPathNodeIds;
    double animationProgress = 0.0;
    double animationDurationMs = 1000.0;
    qint64 animationStartTime = 0;
    QTimer* animationTimer = nullptr;
    QGraphicsPathItem* activeTrackItem = nullptr;
    QGraphicsPathItem* activeGrowthItem = nullptr;
    
    void drawPathGrowthAnimation();

    int findNodeAt(const QPointF& pos);
    int findEdgeAt(const QPointF& pos, QPointF& closestPoint, int& outU, int& outV); 
    QPen edgePenForType(EdgeType type) const;
};