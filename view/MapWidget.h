#pragma once

#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsPixmapItem>
#include <QtWidgets/QGraphicsLineItem>
#include <QtWidgets/QGraphicsPathItem> // 恢复路径项
#include <QtWidgets/QGraphicsTextItem>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtCore/QTimer>
#include <QtCore/QMap>
#include <QtCore/QPointer>
#include <QtCore/QVariantAnimation>
#include <QtCore/QPropertyAnimation> // 恢复动画
#include "../GraphData.h"

// 编辑模式枚举
enum class EditMode {
    None,           // 浏览模式 (导航、动效、气泡)
    ConnectEdge,    // 1. 连边模式
    AddBuilding,    // 2. 新建筑物
    AddGhost        // 3. 新幽灵节点
};

class MapWidget : public QGraphicsView
{
    Q_OBJECT
public:
    explicit MapWidget(QWidget *parent = nullptr);

    void drawMap(const QVector<Node>& nodes, const QVector<Edge>& edges);
    void setBackgroundImage(const QString& path);
    
    // 设置模式
    void setEditMode(EditMode mode);
    EditMode getEditMode() const { return currentMode; }

    // 编辑器接口
    void clearEditTempItems();
    void addEditVisualNode(int id, const QString& name, const QPointF& pos, int type);

    // 恢复：路径高亮接口
    void highlightPath(const QVector<int>& pathNodeIds, double animationDuration = 1.0);
    void clearPathHighlight();

signals:
    // 导航信号 (View Mode)
    void nodeClicked(int nodeId, QString name, bool isLeftClick); // 恢复：带Name的旧信号
    
    // 编辑信号 (Edit Mode)
    void nodeEditClicked(int nodeId, bool isCtrlPressed); // 编辑点击
    void emptySpaceClicked(double x, double y);
    void edgeConnectionRequested(int idA, int idB);
    void nodeMoved(int id, double newX, double newY);
    void undoRequested(); 

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override; // 恢复
    void leaveEvent(QEvent *event) override;        // 恢复

private slots:
    void onAnimationTick();      // 恢复：路径动画
    void resumeHoverAnimations(); // 恢复：气泡动画恢复

private:
    QGraphicsScene* scene;
    QGraphicsPixmapItem* backgroundItem = nullptr;
    
    // 数据缓存
    QVector<Node> cachedNodes;
    QVector<Edge> cachedEdges;
    QMap<int, QGraphicsTextItem*> nodeLabelItems;
    
    // 状态
    EditMode currentMode = EditMode::None;
    double currentScale = 1.0;
    
    // --- 编辑器状态 ---
    bool isMiddlePanning = false;
    QPoint lastPanPos;
    bool isNodeDragging = false;
    int draggingNodeId = -1;
    QPointF lastScenePos;
    int connectFirstNodeId = -1;
    QVector<QGraphicsItem*> editTempItems; // 编辑时的临时项

    // --- 恢复：悬停气泡相关 ---
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
    void startHoverAppearAnimation();
    void pauseHoverAnimations();
    QColor withAlpha(const QColor& c, int alpha);
    QColor nodeBaseColor(const Node& node) const;
    QColor edgeBaseColor(const Edge& edge) const;

    // --- 恢复：路径动画相关 ---
    QVector<int> currentPathNodeIds;
    double animationProgress = 0.0;
    double animationDurationMs = 1000.0;
    qint64 animationStartTime = 0;
    QTimer* animationTimer = nullptr;
    void drawPathGrowthAnimation();

    // 辅助
    int findNodeAt(const QPointF& pos);
    int findEdgeAt(const QPointF& pos, QPointF& closestPoint, int& outU, int& outV); // 恢复
    QPen edgePenForType(EdgeType type) const;
};