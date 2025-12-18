#include "MapWidget.h"
// Module-qualified includes
#include <QtWidgets/QGraphicsEllipseItem>
#include <QtWidgets/QGraphicsLineItem>
#include <QtWidgets/QGraphicsTextItem>
#include "HoverBubble.h"
#include "../model/MapEditor.h"
#include <QtGui/QMouseEvent>
#include <QtCore/QDebug>
#include <cmath>
#include <QtWidgets/QGraphicsPixmapItem>
#include <QtGui/QPixmap>
#include <QtGui/QResizeEvent>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QApplication>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <algorithm>
#include <QtCore/QEasingCurve>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QParallelAnimationGroup>
#include <limits>

MapWidget::MapWidget(QWidget *parent) : QGraphicsView(parent)
{
    scene = new QGraphicsScene(this);
    this->setScene(scene);
    this->setRenderHint(QPainter::Antialiasing);
    this->setBackgroundBrush(QBrush(QColor(240, 240, 245)));
    this->setMouseTracking(true);
    
    // 启用手势拖动，提供类似 Google Maps 的平滑拖拽体验
    this->setDragMode(QGraphicsView::ScrollHandDrag);
    
    mapEditor = new MapEditor(this);
    
    animationTimer = new QTimer(this);
    connect(animationTimer, &QTimer::timeout, this, &MapWidget::onAnimationTick);

    hoverResumeTimer = new QTimer(this);
    hoverResumeTimer->setSingleShot(true);
    connect(hoverResumeTimer, &QTimer::timeout, this, &MapWidget::resumeHoverAnimations);
}

void MapWidget::drawMap(const QVector<Node>& nodes, const QVector<Edge>& edges)
{
    // 清除除了背景图以外的所有项目
    auto items = scene->items();
    for (auto it : items) {
        if (it != backgroundItem && it->zValue() > -50) { // 保护 Z < -50 的背景
            scene->removeItem(it);
            delete it;
        }
    }
    cachedNodes = nodes;
    cachedEdges = edges;
    nodeLabelItems.clear();

    QMap<int, Node> nodeMap;
    for(const auto& node : nodes) nodeMap.insert(node.id, node);

    // 绘制连边
    for(const auto& edge : edges) {
        if(nodeMap.contains(edge.u) && nodeMap.contains(edge.v)) {
            Node start = nodeMap[edge.u];
            Node end = nodeMap[edge.v];
            QPen edgePen = edgePenForType(edge.type);
            auto line = scene->addLine(start.x, start.y, end.x, end.y, edgePen);
            line->setZValue(10);
        }
    }

    QBrush nodeBrush(QColor("white"));
    QPen nodePen(QColor("#2C3E50"));
    nodePen.setWidth(2);

    // 绘制节点
    for(const auto& node : nodes) {
        bool isGhost = (node.type == NodeType::Ghost);
        
        // 编辑模式下显示幽灵节点，否则隐藏
        if (isGhost && !editMode) {
            continue;
        }
        
        double r = isGhost ? 5.0 : 8.0;
        QPen p = isGhost ? QPen(QColor(140, 140, 160)) : nodePen;
        QBrush b = isGhost ? QBrush(QColor(180, 180, 200, 160)) : nodeBrush;
        
        auto item = scene->addEllipse(node.x - r, node.y - r, 2*r, 2*r, p, b);
        item->setZValue(isGhost ? 9 : 10);
        item->setData(0, node.id);

        if (!node.name.isEmpty()) {
            auto text = scene->addText(node.name);
            QRectF textBounds = text->boundingRect();
            text->setPos(node.x - textBounds.width()/2.0, node.y + r + 2);
            text->setDefaultTextColor(QColor("#555"));
            text->setZValue(10);
            nodeLabelItems.insert(node.id, text);
        }
    }

    // 不再强制 fitInView，保持底图分辨率
    if (!backgroundItem) {
        // 计算节点包围盒作为后备
        double minX = std::numeric_limits<double>::infinity();
        double minY = std::numeric_limits<double>::infinity();
        double maxX = -std::numeric_limits<double>::infinity();
        double maxY = -std::numeric_limits<double>::infinity();
        for(const auto& node : nodes) {
            minX = std::min(minX, node.x); minY = std::min(minY, node.y);
            maxX = std::max(maxX, node.x); maxY = std::max(maxY, node.y);
        }
        if (std::isfinite(minX)) {
             scene->setSceneRect(minX-50, minY-50, (maxX-minX)+100, (maxY-minY)+100);
        }
    }
}

void MapWidget::setBackgroundImage(const QString& path)
{
    QPixmap px(path);
    if (px.isNull()) {
        qDebug() << "❌ 无法加载底图:" << path;
        return;
    }

    // 移除旧背景
    if (backgroundItem) {
        scene->removeItem(backgroundItem);
        delete backgroundItem;
        backgroundItem = nullptr;
    }

    // 1. 设置 Scene 的大小严格等于图片的物理像素大小
    scene->setSceneRect(0, 0, px.width(), px.height());
    
    // 2. 添加原图，不做任何缩放
    backgroundItem = scene->addPixmap(px);
    
    // 3. 放在最底层
    backgroundItem->setZValue(-100);
    backgroundItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
    backgroundItem->setFlag(QGraphicsItem::ItemIsMovable, false);
    
    qDebug() << "✅ 底图加载完毕, 尺寸:" << px.size() << " SceneRect已设置为:" << scene->sceneRect();
}

void MapWidget::mousePressEvent(QMouseEvent *event)
{
    QPointF scenePos = mapToScene(event->pos());
    
    // 编辑模式：右键断开，左键添加
    if (editMode) {
        if (event->button() == Qt::RightButton) {
            if (mapEditor) mapEditor->resetConnection();
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton && event->modifiers() & Qt::ControlModifier) {
             // Ctrl + 左键：Visible 节点
            emit editPointPicked(scenePos, true);
            // 【修复】显式转换为 int
            addEditVisualNode(-1, QString(), scenePos, static_cast<int>(NodeType::Visible));
            event->accept();
            return;
        }
         if (event->button() == Qt::LeftButton) {
             // 普通左键：Ghost 节点
             emit editPointPicked(scenePos, false);
             // 【修复】显式转换为 int
             addEditVisualNode(-1, QString(), scenePos, static_cast<int>(NodeType::Ghost));
             // 不 return，允许拖拽
         }
    }

    // 检测节点点击
    int nodeId = findNodeAt(scenePos);
    if (nodeId != -1) {
        clearHoverItems();
        QString name;
        for (const auto &n : cachedNodes) if (n.id == nodeId) name = n.name;
        bool isLeft = (event->button() == Qt::LeftButton);
        emit nodeClicked(nodeId, name, isLeft);
        event->accept();
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void MapWidget::wheelEvent(QWheelEvent *event)
{
    const double scaleFactor = 1.15;
    if (event->angleDelta().y() > 0) {
        this->scale(scaleFactor, scaleFactor);
        currentScale *= scaleFactor;
    } else {
        this->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
        currentScale /= scaleFactor;
    }
    event->accept();
}

void MapWidget::mouseMoveEvent(QMouseEvent *event)
{
    // 如果按住鼠标左键，大概率在拖动，暂停悬停计算
    if (event->buttons() & Qt::LeftButton) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    QPointF scenePos = mapToScene(event->pos());
    int hitNode = findNodeAt(scenePos);
    if (hitNode != -1) {
        if (hoveredNodeId != hitNode || hoveredEdgeIndex != -1) {
            clearHoverItems();
            hoveredNodeId = hitNode;
            hoveredEdgeIndex = -1;
            for (const auto& n : cachedNodes) {
                if (n.id == hitNode) { showNodeHoverBubble(n); break; }
            }
        }
        event->accept();
        return;
    }

    QPointF closest; int u=-1,v=-1; int edgeIdx = findEdgeAt(scenePos, closest, u, v);
    if (edgeIdx != -1) {
        if (hoveredEdgeIndex != edgeIdx || hoveredNodeId != -1) {
            clearHoverItems();
            hoveredEdgeIndex = edgeIdx;
            hoveredNodeId = -1;
            showEdgeHoverBubble(cachedEdges[edgeIdx], closest);
        }
        event->accept();
        return;
    }

    if (hoveredNodeId != -1 || hoveredEdgeIndex != -1) {
        hoveredNodeId = -1;
        hoveredEdgeIndex = -1;
        clearHoverItems();
    }

    QGraphicsView::mouseMoveEvent(event);
}

void MapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    QGraphicsView::mouseReleaseEvent(event);
}

void MapWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    if (hoveredNodeId != -1 || hoveredEdgeIndex != -1) {
        hoveredNodeId = -1;
        hoveredEdgeIndex = -1;
        clearHoverItems();
    }
}

void MapWidget::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    pauseHoverAnimations();
    if (hoverResumeTimer) hoverResumeTimer->start(300);
}

// 查找算法和辅助函数
int MapWidget::findNodeAt(const QPointF& pos)
{
    double threshold = 25.0;
    for(const auto& node : cachedNodes) {
        if (node.type == NodeType::Ghost && !editMode) continue;
        double dx = pos.x() - node.x;
        double dy = pos.y() - node.y;
        if (dx*dx + dy*dy < threshold*threshold) return node.id;
    }
    return -1;
}

int MapWidget::findEdgeAt(const QPointF& pos, QPointF& closestPoint, int& outU, int& outV)
{
    if (cachedEdges.isEmpty() || cachedNodes.isEmpty()) return -1;
    QMap<int, Node> nodeMap;
    for (const auto& n : cachedNodes) nodeMap.insert(n.id, n);

    int bestIdx = -1; double bestDist = 1e18; QPointF bestPt;
    int bu=-1, bv=-1;
    const double threshold = 10.0;

    for (int i = 0; i < cachedEdges.size(); ++i) {
        const auto& e = cachedEdges[i];
        if (!nodeMap.contains(e.u) || !nodeMap.contains(e.v)) continue;
        const auto& a = nodeMap[e.u];
        const auto& b = nodeMap[e.v];
        
        QPointF p(pos.x(), pos.y());
        QPointF A(a.x, a.y), B(b.x, b.y);
        QPointF AB = B - A;
        double ab2 = AB.x()*AB.x() + AB.y()*AB.y();
        if (ab2 <= 1e-6) continue;
        double t = ((p.x()-A.x())*AB.x() + (p.y()-A.y())*AB.y()) / ab2;
        t = std::max(0.0, std::min(1.0, t));
        QPointF proj(A.x()+AB.x()*t, A.y()+AB.y()*t);
        double dx = p.x()-proj.x();
        double dy = p.y()-proj.y();
        double dist2 = dx*dx + dy*dy;
        if (dist2 < bestDist) {
            bestDist = dist2; bestPt = proj; bu = e.u; bv = e.v; bestIdx = i;
        }
    }

    if (bestIdx != -1 && std::sqrt(bestDist) <= threshold) {
        closestPoint = bestPt; outU = bu; outV = bv; return bestIdx;
    }
    return -1;
}

void MapWidget::setEditMode(bool enabled)
{
    editMode = enabled;
    drawMap(cachedNodes, cachedEdges);
}

void MapWidget::clearEditTempItems()
{
    for (auto* it : editTempItems) {
        if (it) { scene->removeItem(it); delete it; }
    }
    editTempItems.clear();
}

void MapWidget::addEditVisualNode(int id, const QString& name, const QPointF& pos, int typeInt)
{
    // 将 int 转换为 Enum 用于内部逻辑
    NodeType type = (typeInt == 9) ? NodeType::Ghost : NodeType::Visible;
    
    QPen p(type == NodeType::Ghost ? QColor(140,140,160) : QColor("#2C3E50"));
    QBrush b(type == NodeType::Ghost ? QColor(180,180,200,180) : QColor("white"));
    double r = (type == NodeType::Ghost) ? 5.0 : 8.0;
    auto* item = scene->addEllipse(pos.x()-r, pos.y()-r, 2*r, 2*r, p, b);
    item->setZValue(12);
    editTempItems.append(item);

    if (!name.isEmpty()) {
        auto* text = scene->addText(name);
        QRectF bounds = text->boundingRect();
        text->setPos(pos.x() - bounds.width()/2.0, pos.y() + r + 2);
        editTempItems.append(text);
    }
}

void MapWidget::highlightPath(const QVector<int>& pathNodeIds, double animationDuration)
{
    if (pathNodeIds.isEmpty()) { clearPathHighlight(); return; }
    clearPathHighlight();
    currentPathNodeIds = pathNodeIds;
    animationDurationMs = animationDuration * 1000.0;
    animationProgress = 0.0;
    animationStartTime = QDateTime::currentMSecsSinceEpoch();

    QMap<int, Node> nodeMap;
    for(const auto& node : cachedNodes) nodeMap.insert(node.id, node);

    for (int i = 0; i < pathNodeIds.size() - 1; ++i) {
        int u = pathNodeIds[i];
        int v = pathNodeIds[i + 1];
        if (nodeMap.contains(u) && nodeMap.contains(v)) {
            const Node& a = nodeMap[u];
            const Node& b = nodeMap[v];
            QPen pen(QColor(100, 180, 240, 200));
            pen.setWidth(6);
            pen.setJoinStyle(Qt::RoundJoin);
            pen.setCapStyle(Qt::RoundCap);
            auto line = scene->addLine(a.x, a.y, b.x, b.y, pen);
            line->setZValue(15);
        }
    }
    animationTimer->start(16);
}

void MapWidget::onAnimationTick()
{
    if (currentPathNodeIds.isEmpty()) { animationTimer->stop(); return; }
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - animationStartTime;
    animationProgress = std::min(1.0, elapsed / animationDurationMs);
    if (animationProgress >= 1.0) {
        animationTimer->stop();
        animationProgress = 1.0;
    }
    drawPathGrowthAnimation();
}

void MapWidget::drawPathGrowthAnimation()
{
    auto items = scene->items();
    for (auto item : items) {
        if (item->zValue() == 20) { scene->removeItem(item); delete item; }
    }
    
    QMap<int, Node> nodeMap;
    for(const auto& node : cachedNodes) nodeMap.insert(node.id, node);
    
    if (currentPathNodeIds.size() < 2) return;
    
    QPainterPath growthPath;
    Node first = nodeMap[currentPathNodeIds[0]];
    growthPath.moveTo(first.x, first.y);
    
    double totalSegs = currentPathNodeIds.size() - 1;
    double currentPos = animationProgress * totalSegs;
    int fullSegs = static_cast<int>(currentPos);
    double segProgress = currentPos - fullSegs;
    
    for (int i = 0; i < fullSegs; ++i) {
        Node n = nodeMap[currentPathNodeIds[i+1]];
        growthPath.lineTo(n.x, n.y);
    }
    
    if (fullSegs < totalSegs) {
        Node n1 = nodeMap[currentPathNodeIds[fullSegs]];
        Node n2 = nodeMap[currentPathNodeIds[fullSegs+1]];
        double dx = n2.x - n1.x;
        double dy = n2.y - n1.y;
        growthPath.lineTo(n1.x + dx*segProgress, n1.y + dy*segProgress);
    }
    
    QPen growthPen(QColor(70, 150, 255));
    growthPen.setWidth(5);
    auto pathItem = scene->addPath(growthPath, growthPen);
    pathItem->setZValue(20);
}

QPen MapWidget::edgePenForType(EdgeType type) const
{
    QColor c(200,200,200);
    int width = 3;
    switch (type) {
    case EdgeType::Normal: c = QColor(180,180,180); width = 3; break;
    case EdgeType::Main:   c = QColor(150,180,220); width = 4; break;
    case EdgeType::Path:   c = QColor(120,200,140); width = 3; break;
    case EdgeType::Indoor: c = QColor(220,160,120); width = 3; break;
    default:               c = QColor(200,200,200); width = 3; break;
    }
    QPen pen(c);
    pen.setWidth(width);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    pen.setCosmetic(true);
    return pen;
}

QColor MapWidget::withAlpha(const QColor& c, int alpha) { QColor t=c; t.setAlpha(alpha); return t; }
void MapWidget::clearHoverItems() {
    if (hoverItems.isEmpty()) return;
    for (int nid : hiddenLabelNodeIds) if (nodeLabelItems.contains(nid) && nodeLabelItems[nid]) nodeLabelItems[nid]->setVisible(true);
    hiddenLabelNodeIds.clear();
    for (auto &a : hoverAnims) { if (a) { a->stop(); a->deleteLater(); } }
    hoverAnims.clear();
    for (auto* it : hoverItems) { 
        if (it) { 
             // 简单的淡出逻辑或直接删除
             scene->removeItem(it); delete it; 
        } 
    }
    hoverItems.clear();
}
void MapWidget::showNodeHoverBubble(const Node& node) {
    const QColor basePenColor = QColor("#2C3E50");
    const QColor bubbleColor = withAlpha(basePenColor, 64);
    HoverBubble* hb = new HoverBubble();
    hb->setIsEdge(false);
    hb->setBaseColor(bubbleColor);
    hb->setContent(node.name, node.description);
    hb->setCenterAt(QPointF(node.x, node.y));
    hb->setZValue(100);
    scene->addItem(hb);
    hoverItems.push_back(hb);
    if (nodeLabelItems.contains(node.id) && nodeLabelItems[node.id]) {
        nodeLabelItems[node.id]->setVisible(false);
        hiddenLabelNodeIds.push_back(node.id);
    }
    startHoverAppearAnimation();
}
void MapWidget::showEdgeHoverBubble(const Edge& edge, const QPointF& closestPoint) {
    const QColor baseEdgeColor = edgePenForType(edge.type).color();
    const QColor bubbleColor = withAlpha(baseEdgeColor, 64);
    
    QMap<int, Node> nodeMap; for (const auto& n : cachedNodes) nodeMap.insert(n.id, n);
    if (!nodeMap.contains(edge.u) || !nodeMap.contains(edge.v)) return;
    Node a = nodeMap[edge.u]; Node b = nodeMap[edge.v];
    QPointF A(a.x, a.y), B(b.x, b.y);
    
    HoverBubble* hb = new HoverBubble();
    hb->setIsEdge(true);
    hb->setBaseColor(bubbleColor);
    hb->setContent(edge.name.isEmpty() ? QString("%1-%2").arg(edge.u).arg(edge.v) : edge.name, edge.description);
    double dx = B.x()-A.x(); double dy = B.y()-A.y();
    hb->setAngle(std::atan2(dy, dx) * 180.0 / M_PI);
    hb->setEdgeLine(A, B);
    hb->setZValue(101);
    scene->addItem(hb);
    hoverItems.push_back(hb);
    startHoverAppearAnimation();
}

void MapWidget::startHoverAppearAnimation() {
    for (auto* it : hoverItems) {
        if (!it) continue;
        HoverBubble* hb = qgraphicsitem_cast<HoverBubble*>(it);
        if (!hb) continue;
        hb->setOpacity(0.0);
        hb->setBubbleScale(0.96);
        auto* a_op = new QPropertyAnimation(hb, "opacity", this);
        a_op->setDuration(140); a_op->setStartValue(0.0); a_op->setEndValue(1.0);
        auto* a_sc = new QPropertyAnimation(hb, "bubbleScale", this);
        a_sc->setDuration(140); a_sc->setStartValue(0.96); a_sc->setEndValue(1.0);
        auto* group = new QParallelAnimationGroup(this);
        group->addAnimation(a_op); group->addAnimation(a_sc);
        connect(group, &QAbstractAnimation::finished, group, &QObject::deleteLater);
        hoverAnims.push_back(QPointer<QAbstractAnimation>(group));
        group->start();
    }
}
void MapWidget::pauseHoverAnimations() {
    for (auto &a : hoverAnims) if (a && a->state() == QAbstractAnimation::Running) a->pause();
}
void MapWidget::resumeHoverAnimations() {
    for (auto &a : hoverAnims) if (a && a->state() == QAbstractAnimation::Paused) a->resume();
}
void MapWidget::clearPathHighlight() {
    if (animationTimer->isActive()) animationTimer->stop();
    animationProgress = 0.0;
    currentPathNodeIds.clear();
    auto items = scene->items();
    for (auto it : items) {
        if (it->zValue() >= 15.0 && it->zValue() <= 20.0) { scene->removeItem(it); delete it; }
    }
    this->viewport()->update();
}
QPainterPath MapWidget::buildSmoothPath(const QVector<QPointF>& pts) const { return QPainterPath(); }
QPainterPath MapWidget::buildPartialPathFromLength(const QPainterPath& fullPath, double length) const { return QPainterPath(); }

void MapWidget::setStartNode(int nodeId) {}
void MapWidget::setEndNode(int nodeId) {}
QColor MapWidget::nodeBaseColor(const Node& node) const { return QColor("#2C3E50"); }
QColor MapWidget::edgeBaseColor(const Edge& edge) const { return QColor(200,200,200); }