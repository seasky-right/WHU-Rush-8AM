#pragma once

// Qt module-qualified includes for clarity
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsScene>
#include <QtGui/QWheelEvent>
#include <QtGui/QResizeEvent>
#include <QtWidgets/QGraphicsPixmapItem>
#include <QtWidgets/QGraphicsLineItem>
#include <QtGui/QPixmap>
#include <QtCore/QTimer>
#include <QtCore/QEvent>
#include <QtCore/QMap>
#include <QtCore/QPointer>
#include <QtWidgets/QGraphicsTextItem>
#include <QtCore/QVariantAnimation>
#include <QtCore/QAbstractAnimation>
#include <QtCore/QVector>
#include <QtCore/QPointF>
#include <QtGui/QColor>
#include "../GraphData.h"
// Map data editor integration
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
    // 编辑模式：用于采集地图数据（点击生成节点/边）
    void setEditMode(bool enabled);
    bool isEditMode() const { return editMode; }
    MapEditor* getEditor() const { return mapEditor; }
    // 编辑模式临时节点管理（供外部调用以添加可视节点）
    void clearEditTempItems();
    void addEditVisualNode(int id, const QString& name, const QPointF& pos, int type);
    
    // 路径高亮和动画接口
    void highlightPath(const QVector<int>& pathNodeIds, double animationDuration = 1.0);
    void clearPathHighlight();  // 公开接口，用于清除路径高亮

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

private:
    QGraphicsScene* scene;
    QVector<Node> cachedNodes;
    QVector<Edge> cachedEdges;
    QMap<int, QGraphicsTextItem*> nodeLabelItems; // 原始节点标签
    int findNodeAt(const QPointF& pos);
    int findEdgeAt(const QPointF& pos, QPointF& closestPoint, int& outU, int& outV);
    QGraphicsPixmapItem* backgroundItem = nullptr;
    double currentScale = 1.0;  // 当前缩放因子
    
    // 鼠标拖动状态
    bool isDragging = false;
    QPoint lastMousePos;
    
    // 悬停气泡相关
    int hoveredNodeId = -1;
    int hoveredEdgeIndex = -1; // 在 cachedEdges 中的下标
    QVector<QGraphicsItem*> hoverItems; // 存放临时的气泡与文本
    QVector<int> hiddenLabelNodeIds; // 被临时隐藏的原始标签
    QVector<QPointer<QAbstractAnimation>> hoverAnims; // 悬停动画(使用 QPointer 防止悬空)
    void clearHoverItems();
    void showNodeHoverBubble(const Node& node);
    void showEdgeHoverBubble(const Edge& edge, const QPointF& closestPoint);
    QColor withAlpha(const QColor& c, int alpha);
    void startHoverAppearAnimation();
    void pauseHoverAnimations();
    void resumeHoverAnimations();
    QTimer* hoverResumeTimer = nullptr; // used to resume after resize
    QColor nodeBaseColor(const Node& node) const;
    QColor edgeBaseColor(const Edge& edge) const;
    quint64 hoverUidCounter = 1;
    
    // 路径动画相关
    QVector<int> currentPathNodeIds;
    QGraphicsLineItem* pathGrowthLine = nullptr;  // 生长中的路径线
    QGraphicsPathItem* currentRouteItem = nullptr; // 当前完整路径项（平滑曲线）
    QTimer* animationTimer = nullptr;
    double animationProgress = 0.0;  // 0 到 1.0
    double animationDurationMs = 1000.0;  // 动画持续时间（毫秒）
    qint64 animationStartTime = 0;
    
    // 清除路径高亮和动画（私有实现）
    void drawPathGrowthAnimation();

    // 辅助：根据类型返回边颜色/宽度
    QPen edgePenForType(int type) const;
    // 辅助：从点序列构建平滑路径（Catmull-Rom 转 Bezier）
    QPainterPath buildSmoothPath(const QVector<QPointF>& pts) const;
    // 辅助：按长度截取部分平滑路径，用于增长动画
    QPainterPath buildPartialPathFromLength(const QPainterPath& fullPath, double length) const;

    // 编辑模式
    bool editMode = false;
    MapEditor* mapEditor = nullptr;
    // 临时编辑可视节点（包括幽灵节点）
    QVector<QGraphicsItem*> editTempItems;
};
