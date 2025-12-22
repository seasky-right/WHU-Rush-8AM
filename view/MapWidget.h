#pragma once

#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsPixmapItem>
#include <QtWidgets/QGraphicsLineItem>
#include <QtWidgets/QGraphicsPathItem> 
#include <QtWidgets/QGraphicsTextItem>
#include <QtWidgets/QGraphicsEllipseItem> 
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtCore/QTimer>
#include <QtCore/QMap>
#include <QtCore/QPointer>
#include <QtCore/QVariantAnimation>
#include <QtCore/QPropertyAnimation>
#include "../GraphData.h"
#include "WeatherOverlay.h" 

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

    void setEditable(bool editable) { m_isEditable = editable; }

    void drawMap(const QVector<Node>& nodes, const QVector<Edge>& edges);
    void setBackgroundImage(const QString& path);
    void setEditMode(EditMode mode);
    EditMode getEditMode() const { return currentMode; }

    void setWeather(Weather weatherType);
    void setShowGhostNodes(bool show);
    void setShowEdges(bool show);              // 新增：控制是否显示边
    void setNodeSizeMultiplier(double mult);   // 新增：节点大小倍数

    void clearEditTempItems();
    void addEditVisualNode(int id, const QString& name, const QPointF& pos, int type);

    void highlightPath(const QVector<int>& pathNodeIds, double animationDuration = 1.0);
    void clearPathHighlight();
    void setActiveEdge(int u, int v);

    void pauseHoverAnimations();
    void resumeHoverAnimations();

signals:
    void nodeClicked(int nodeId, QString name, bool isLeftClick);
    void nodeEditClicked(int nodeId, bool isCtrlPressed);
    void emptySpaceClicked(double x, double y);
    void edgeConnectionRequested(int idA, int idB);
    void nodeMoved(int id, double x, double y);
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

private:
    QGraphicsScene* scene;
    QGraphicsPixmapItem* backgroundItem = nullptr;
    QString m_bgPath; 

    WeatherOverlay* weatherOverlay = nullptr;
    Weather m_currentWeatherState = Weather::Sunny;
    bool m_showGhostNodes = false; 
    bool m_showEdges = true;           // 新增：是否显示边
    double m_nodeSizeMultiplier = 1.0; // 新增：节点大小倍数

    bool m_isEditable = false;

    QVector<Node> cachedNodes;
    QVector<Edge> cachedEdges;
    
    int findNodeAt(const QPointF& pos);
    int findEdgeAt(const QPointF& pos, QPointF& closestPoint, int& outU, int& outV);
    QPen edgePenForType(EdgeType type) const;

    // --- 状态 ---
    EditMode currentMode = EditMode::None;
    bool isNodeDragging = false;
    int draggingNodeId = -1;
    bool isMiddlePanning = false;
    QPoint lastPanPos;
    QPointF lastScenePos;
    
    int connectFirstNodeId = -1;
    QVector<QGraphicsItem*> editTempItems; 

    int activeEdgeU = -1;
    int activeEdgeV = -1;

    // --- 悬停与气泡 ---
    int hoveredNodeId = -1;
    int hoveredEdgeIndex = -1;
    double currentBubbleScale = 1.0;

    QVector<QGraphicsItem*> hoverItems;
    QVector<QGraphicsItem*> dyingItems; 
    
    QMap<int, QGraphicsEllipseItem*> nodeGraphicsItems; 
    QMap<int, QGraphicsTextItem*> nodeLabelItems; 
    QMap<long long, QGraphicsLineItem*> edgeGraphicsItems; 
    
    QMap<int, QVector<long long>> nodeConnectedEdgeKeys;

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
    
    QColor withAlpha(const QColor& c, int alpha);

    // === 【核心修改】新增的高亮函数声明 ===
    void updateNodeHighlight(int nodeId, bool highlight);
    
    // --- 路径动画 ---
    QVector<int> currentPathNodeIds;
    double animationProgress = 0.0;
    double animationDurationMs = 1000.0;
    qint64 animationStartTime = 0;
    QTimer* animationTimer;
    QGraphicsPathItem* activeTrackItem = nullptr;
    QGraphicsPathItem* activeGrowthItem = nullptr;
    
    void drawPathGrowthAnimation();
};