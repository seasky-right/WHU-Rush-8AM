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
    mapEditor = new MapEditor(this);
    
    // 初始化动画计时器
    animationTimer = new QTimer(this);
    connect(animationTimer, &QTimer::timeout, this, &MapWidget::onAnimationTick);

    // hover resume timer
    hoverResumeTimer = new QTimer(this);
    hoverResumeTimer->setSingleShot(true);
    connect(hoverResumeTimer, &QTimer::timeout, this, &MapWidget::resumeHoverAnimations);
}

void MapWidget::drawMap(const QVector<Node>& nodes, const QVector<Edge>& edges)
{
    // 仅移除 ZValue > 0 的项（保留背景等低 Z 值项）
    auto items = scene->items();
    for (auto it : items) {
        if (it->zValue() > 0) {
            scene->removeItem(it);
            delete it;
        }
    }
    cachedNodes = nodes;
    cachedEdges = edges;
    nodeLabelItems.clear();

    // 基础道路按类型设置样式

    QMap<int, Node> nodeMap;
    for(const auto& node : nodes) nodeMap.insert(node.id, node);

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

    // 计算节点包围盒
    double minX = std::numeric_limits<double>::infinity();
    double minY = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity();

    for(const auto& node : nodes) {
        bool isGhost = (node.type == 9);
        // 编辑模式：幽灵节点也可见；普通模式仍隐藏幽灵节点
        if (isGhost && !editMode) {
            minX = std::min(minX, node.x);
            minY = std::min(minY, node.y);
            maxX = std::max(maxX, node.x);
            maxY = std::max(maxY, node.y);
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

        // 累积包围盒
        minX = std::min(minX, node.x);
        minY = std::min(minY, node.y);
        maxX = std::max(maxX, node.x);
        maxY = std::max(maxY, node.y);
    }

    // 设置场景矩形，避免背景图对 fitInView 的影响
    if (!nodes.isEmpty() && std::isfinite(minX) && std::isfinite(minY) && std::isfinite(maxX) && std::isfinite(maxY)) {
        const double margin = 40.0;
        QRectF nodesRect(QPointF(minX, minY), QPointF(maxX, maxY));
        nodesRect = nodesRect.adjusted(-margin, -margin, margin, margin);
        scene->setSceneRect(nodesRect);
        this->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
        currentScale = 1.0;
    }
}

void MapWidget::mousePressEvent(QMouseEvent *event)
{
    QPointF scenePos = mapToScene(event->pos());
    // 编辑模式：采集地图数据
    if (editMode) {
        if (event->button() == Qt::RightButton) {
            if (mapEditor) mapEditor->resetConnection();
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton) {
            bool isCtrl = QApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
            // 发射信号，由 UI 收集更多字段后调用 MapEditor::createNode
            emit editPointPicked(scenePos, isCtrl);
            // 立即放置一个临时可视点，便于确认位置（幽灵节点也显示）
            addEditVisualNode(-1, QString(), scenePos, isCtrl ? 0 : 9);
            event->accept();
            return;
        }
        // 其他按键直接忽略
    }
    int nodeId = findNodeAt(scenePos);

    if (nodeId != -1) {
        // 清理任何悬停临时项，避免动画或悬浮项在后续处理或槽中被访问导致竞态
        clearHoverItems();

        QString name = "";
        for (const auto &n : cachedNodes) if (n.id == nodeId) name = n.name;

        bool isLeft = (event->button() == Qt::LeftButton);
        emit nodeClicked(nodeId, name, isLeft);

        qDebug() << "选中节点:" << name << (isLeft ? "[起点]" : "[终点]");
        event->accept();
        return;
    }

    // 左键未点中节点：进入拖动模式
    if (event->button() == Qt::LeftButton) {
        // pause hover animations while dragging to avoid races and CPU
        pauseHoverAnimations();
        isDragging = true;
        lastMousePos = event->pos();
        event->accept();
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void MapWidget::wheelEvent(QWheelEvent *event)
{
    // 逐级缩放：向上放大，向下缩小；底图和节点一起缩放
    double scaleFactor = 1.15;  // 每次缩放的倍数
    if (event->angleDelta().y() > 0) {
        // 向上：放大
        currentScale *= scaleFactor;
    } else {
        // 向下：缩小（但不低于原始尺寸）
        currentScale /= scaleFactor;
        if (currentScale < 1.0) currentScale = 1.0;
    }

    // 设置新的缩放变换（相对于当前 currentScale）
    this->resetTransform();
    this->scale(currentScale, currentScale);

    event->accept();
}

void MapWidget::mouseMoveEvent(QMouseEvent *event)
{
    // 如果在拖动模式，平移视图
    if (isDragging) {
        QPoint delta = event->pos() - lastMousePos;
        this->horizontalScrollBar()->setValue(this->horizontalScrollBar()->value() - delta.x());
        this->verticalScrollBar()->setValue(this->verticalScrollBar()->value() - delta.y());
        lastMousePos = event->pos();
        event->accept();
        return;
    }
    
    // 悬停命中测试（节点优先，其次路径）
    QPointF scenePos = mapToScene(event->pos());
    int hitNode = findNodeAt(scenePos);
    if (hitNode != -1) {
        if (hoveredNodeId != hitNode || hoveredEdgeIndex != -1) {
            clearHoverItems();
            hoveredNodeId = hitNode;
            hoveredEdgeIndex = -1;
            // 找到节点对象
            for (const auto& n : cachedNodes) {
                if (n.id == hitNode) { showNodeHoverBubble(n); break; }
            }
        }
        event->accept();
        return;
    }

    // 节点没命中，则检测路径
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

    // 未命中任何对象，清理悬停项
    if (hoveredNodeId != -1 || hoveredEdgeIndex != -1) {
        hoveredNodeId = -1;
        hoveredEdgeIndex = -1;
        clearHoverItems();
    }

    QGraphicsView::mouseMoveEvent(event);
}

void MapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    // 停止拖动模式
    if (event->button() == Qt::LeftButton && isDragging) {
        isDragging = false;
        // resume hover animations after drag
        resumeHoverAnimations();
        event->accept();
        return;
    }

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

void MapWidget::setBackgroundImage(const QString& path)
{
    QPixmap px(path);
    if (px.isNull()) return;

    // 移除旧的背景项
    if (backgroundItem) {
        scene->removeItem(backgroundItem);
        delete backgroundItem;
        backgroundItem = nullptr;
    }
    QRectF sr = scene->sceneRect();
    if (!sr.isEmpty()) {
        // 使用 KeepAspectRatioByExpanding 以覆盖整个 sceneRect，然后裁剪中心
        QSize targetSize(qMax(1, int(sr.width())), qMax(1, int(sr.height())));
        QPixmap scaled = px.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        backgroundItem = scene->addPixmap(scaled);
        // 使背景铺满 sceneRect，以中心对齐
        QPointF topLeft(sr.center().x() - scaled.width()/2.0, sr.center().y() - scaled.height()/2.0);
        backgroundItem->setPos(topLeft);
    } else {
        // 若无 sceneRect，则用原图并设置 sceneRect 与之匹配
        backgroundItem = scene->addPixmap(px);
        scene->setSceneRect(QRectF(QPointF(0,0), QSizeF(px.size())));
        backgroundItem->setPos(0,0);
    }

    // 确保背景在最底层
    backgroundItem->setZValue(-100);
    backgroundItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
    backgroundItem->setFlag(QGraphicsItem::ItemIsMovable, false);
    backgroundItem->setAcceptedMouseButtons(Qt::NoButton);
}

void MapWidget::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    // resize 时无需特殊处理，view 的缩放变换会自动影响所有 scene 项包括背景
    // 暂停悬停动画，短延迟后恢复，避免在 resize 过程中的竞态和高 CPU
    pauseHoverAnimations();
    if (hoverResumeTimer) hoverResumeTimer->start(300);
}

int MapWidget::findNodeAt(const QPointF& pos)
{
    double threshold = 25.0;

    for(const auto& node : cachedNodes) {
        double dx = pos.x() - node.x;
        double dy = pos.y() - node.y;
        double dist = std::sqrt(dx*dx + dy*dy);

        if (dist < threshold) {
            return node.id;
        }
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
    // 命中阈值（像素）
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

void MapWidget::setStartNode(int nodeId) {
}

void MapWidget::setEndNode(int nodeId) {
}

void MapWidget::setEditMode(bool enabled)
{
    editMode = enabled;
    // 切换时重绘（让幽灵节点在编辑模式下可见）
    if (!cachedNodes.isEmpty() || !cachedEdges.isEmpty()) {
        drawMap(cachedNodes, cachedEdges);
    }
}

void MapWidget::clearEditTempItems()
{
    for (auto* it : editTempItems) {
        if (it) {
            scene->removeItem(it);
            delete it;
        }
    }
    editTempItems.clear();
}

void MapWidget::addEditVisualNode(int id, const QString& name, const QPointF& pos, int type)
{
    QPen p(type == 9 ? QColor(140,140,160) : QColor("#2C3E50"));
    QBrush b(type == 9 ? QColor(180,180,200,180) : QColor("white"));
    double r = (type == 9) ? 5.0 : 8.0;
    auto* item = scene->addEllipse(pos.x()-r, pos.y()-r, 2*r, 2*r, p, b);
    item->setZValue(12);
    item->setData(0, id);
    editTempItems.append(item);

    if (!name.isEmpty()) {
        auto* text = scene->addText(name);
        QRectF bounds = text->boundingRect();
        text->setPos(pos.x() - bounds.width()/2.0, pos.y() + r + 2);
        text->setDefaultTextColor(QColor("#555"));
        text->setZValue(12);
        editTempItems.append(text);
    }
}

void MapWidget::highlightPath(const QVector<int>& pathNodeIds, double animationDuration)
{
    if (pathNodeIds.isEmpty()) {
        clearPathHighlight();
        return;
    }

    // 清除旧的动画
    clearPathHighlight();

    // 保存路径节点
    currentPathNodeIds = pathNodeIds;
    animationDurationMs = animationDuration * 1000.0;  // 转换为毫秒
    animationProgress = 0.0;
    animationStartTime = QDateTime::currentMSecsSinceEpoch();

    // 绘制完整路径的高亮背景（逐段直线）
    QMap<int, Node> nodeMap;
    for(const auto& node : cachedNodes) {
        nodeMap.insert(node.id, node);
    }
    for (int i = 0; i < pathNodeIds.size() - 1; ++i) {
        int u = pathNodeIds[i];
        int v = pathNodeIds[i + 1];
        if (nodeMap.contains(u) && nodeMap.contains(v)) {
            const Node& a = nodeMap[u];
            const Node& b = nodeMap[v];
            QPen pen(QColor(100, 180, 240, 200));  // 浅蓝
            pen.setWidth(6);
            pen.setJoinStyle(Qt::RoundJoin);
            pen.setCapStyle(Qt::RoundCap);
            auto line = scene->addLine(a.x, a.y, b.x, b.y, pen);
            line->setZValue(15);
        }
    }

    // 启动动画定时器
    animationTimer->start(16);  // 约60 FPS
}

void MapWidget::onAnimationTick()
{
    if (currentPathNodeIds.isEmpty()) {
        animationTimer->stop();
        return;
    }
    
    // 计算动画进度
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - animationStartTime;
    animationProgress = std::min(1.0, elapsed / animationDurationMs);
    
    // 如果动画完成，停止定时器
    if (animationProgress >= 1.0) {
        animationTimer->stop();
        animationProgress = 1.0;
    }
    
    // 重绘路径生长动画
    drawPathGrowthAnimation();
}

void MapWidget::drawPathGrowthAnimation()
{
    // 首先删除之前的生长线（ZValue=19/20 的所有项）
    auto items = scene->items();
    for (auto item : items) {
        double z = item->zValue();
        if (z == 20 || z == 19) {
            scene->removeItem(item);
            delete item;
        }
    }
    
    QMap<int, Node> nodeMap;
    for(const auto& node : cachedNodes) {
        nodeMap.insert(node.id, node);
    }
    
    // 根据动画进度计算应该绘制到哪里
    double totalDistance = 0.0;
    QVector<double> segmentDistances;
    
    // 首先计算所有路径段的距离
    for (int i = 0; i < currentPathNodeIds.size() - 1; ++i) {
        int u = currentPathNodeIds[i];
        int v = currentPathNodeIds[i + 1];
        
        if (nodeMap.contains(u) && nodeMap.contains(v)) {
            Node startNode = nodeMap[u];
            Node endNode = nodeMap[v];
            
            double dx = endNode.x - startNode.x;
            double dy = endNode.y - startNode.y;
            double dist = std::sqrt(dx * dx + dy * dy);
            
            segmentDistances.append(dist);
            totalDistance += dist;
        }
    }
    
    if (totalDistance <= 0.0 || segmentDistances.isEmpty()) {
        return;
    }
    
    // 计算当前应该绘制到的距离
    double currentDistance = animationProgress * totalDistance;
    double accumulatedDistance = 0.0;
    int currentSegment = -1;
    double segmentProgress = 0.0;
    
    // 找到当前在哪一段
    for (int i = 0; i < segmentDistances.size(); ++i) {
        if (accumulatedDistance + segmentDistances[i] >= currentDistance) {
            currentSegment = i;
            segmentProgress = (currentDistance - accumulatedDistance) / segmentDistances[i];
            break;
        }
        accumulatedDistance += segmentDistances[i];
    }
    
    if (currentSegment < 0) {
        currentSegment = segmentDistances.size() - 1;
        segmentProgress = 1.0;
    }
    
    // 绘制从起点到当前位置的生长路径（分段直线）
    if (currentSegment >= 0 && currentPathNodeIds.size() > currentSegment + 1) {
        Node startNode = nodeMap[currentPathNodeIds[0]];
        
        // 计算当前段的结束点（部分）
        Node currentSegmentStart = nodeMap[currentPathNodeIds[currentSegment]];
        Node currentSegmentEnd = nodeMap[currentPathNodeIds[currentSegment + 1]];
        
        double dx = currentSegmentEnd.x - currentSegmentStart.x;
        double dy = currentSegmentEnd.y - currentSegmentStart.y;
        
        double currentX = currentSegmentStart.x + dx * segmentProgress;
        double currentY = currentSegmentStart.y + dy * segmentProgress;
        
        // 构造从起点到当前位置的路径
        QPainterPath growthPath;
        growthPath.moveTo(startNode.x, startNode.y);
        
        // 绘制所有已完成的路径段
        for (int i = 1; i <= currentSegment; ++i) {
            if (nodeMap.contains(currentPathNodeIds[i])) {
                Node segNode = nodeMap[currentPathNodeIds[i]];
                growthPath.lineTo(segNode.x, segNode.y);
            }
        }
        
        // 绘制当前段的部分
        growthPath.lineTo(currentX, currentY);
        
        // 使用更鲜艳的蓝色绘制前景生长线
        QPen growthPen(QColor(70, 150, 255));
        growthPen.setWidth(5);
        growthPen.setCapStyle(Qt::RoundCap);
        growthPen.setJoinStyle(Qt::RoundJoin);
        
        auto pathItem = scene->addPath(growthPath, growthPen);
        pathItem->setZValue(20);
    }
    
    // 触发重绘
    this->viewport()->update();
}

QPen MapWidget::edgePenForType(int type) const
{
    QColor c(200,200,200);
    int width = 3;
    switch (type) {
    case 0: c = QColor(180,180,180); width = 3; break;      // 普通道路
    case 1: c = QColor(150,180,220); width = 4; break;      // 主干道
    case 2: c = QColor(120,200,140); width = 3; break;      // 小径/绿道
    case 3: c = QColor(220,160,120); width = 3; break;      // 楼内连线
    default: c = QColor(200,200,200); width = 3; break;
    }
    QPen pen(c);
    pen.setWidth(width);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    pen.setCosmetic(true);
    return pen;
}

// Catmull-Rom to Bezier conversion for smooth path
QPainterPath MapWidget::buildSmoothPath(const QVector<QPointF>& pts) const
{
    QPainterPath path;
    if (pts.isEmpty()) return path;
    path.moveTo(pts[0]);

    if (pts.size() == 2) {
        path.lineTo(pts[1]);
        return path;
    }

    // Tension parameter for Catmull-Rom (0.5 standard)
    const double t = 0.5;
    auto addSegment = [&](const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3){
        // Control points derived from Catmull-Rom
        QPointF c1 = p1 + (p2 - p0) * (t / 3.0);
        QPointF c2 = p2 - (p3 - p1) * (t / 3.0);
        path.cubicTo(c1, c2, p2);
    };

    // For endpoints, duplicate boundary points to compute controls
    for (int i = 0; i < pts.size() - 1; ++i) {
        QPointF p0 = (i == 0) ? pts[0] : pts[i-1];
        QPointF p1 = pts[i];
        QPointF p2 = pts[i+1];
        QPointF p3 = (i+2 < pts.size()) ? pts[i+2] : pts[i+1];
        // Build cubic segment from p1 to p2
        QPointF c1 = p1 + (p2 - p0) * (t / 3.0);
        QPointF c2 = p2 - (p3 - p1) * (t / 3.0);
        path.cubicTo(c1, c2, p2);
    }

    return path;
}

QPainterPath MapWidget::buildPartialPathFromLength(const QPainterPath& fullPath, double length) const
{
    QPainterPath out;
    if (fullPath.isEmpty() || length <= 0.0) return out;

    // 采用折线近似逐段截取
    double acc = 0.0;
    bool started = false;
    for (const QPolygonF& poly : fullPath.toSubpathPolygons()) {
        if (poly.isEmpty()) continue;
        QPointF prev = poly[0];
        if (!started) { out.moveTo(prev); started = true; }
        for (int i = 1; i < poly.size(); ++i) {
            QPointF cur = poly[i];
            double dx = cur.x() - prev.x();
            double dy = cur.y() - prev.y();
            double segLen = std::sqrt(dx*dx + dy*dy);
            if (acc + segLen >= length) {
                double remain = length - acc;
                double ratio = (segLen > 1e-6) ? (remain / segLen) : 0.0;
                QPointF cut(prev.x() + dx*ratio, prev.y() + dy*ratio);
                out.lineTo(cut);
                return out;
            } else {
                out.lineTo(cur);
                acc += segLen;
                prev = cur;
            }
        }
    }
    return out;
}

void MapWidget::clearPathHighlight()
{
    // 停止动画并重置进度
    if (animationTimer && animationTimer->isActive()) animationTimer->stop();
    animationProgress = 0.0;

    // 清除路径模型
    currentPathNodeIds.clear();

    // 移除旧的生长线
    if (pathGrowthLine) {
        scene->removeItem(pathGrowthLine);
        delete pathGrowthLine;
        pathGrowthLine = nullptr;
    }

    // 移除高亮路径项（ZValue 15/16/20 等）
    auto items = scene->items();
    for (auto it : items) {
        double z = it->zValue();
        if (z >= 15.0 && z <= 20.0) {
            scene->removeItem(it);
            delete it;
        }
    }

    // 清理记录的当前完整路径项
    if (currentRouteItem) {
        scene->removeItem(currentRouteItem);
        delete currentRouteItem;
        currentRouteItem = nullptr;
    }

    this->viewport()->update();
}

// --------------------- 悬停绘制辅助 ---------------------

QColor MapWidget::withAlpha(const QColor& c, int alpha)
{
    QColor t = c; t.setAlpha(alpha); return t;
}

void MapWidget::clearHoverItems()
{
    qDebug() << "clearHoverItems() called. hoverItems:" << hoverItems.size() << "hoverAnims:" << hoverAnims.size();
    if (hoverItems.isEmpty()) return;

    // 先恢复被隐藏的原始节点标签
    for (int nid : hiddenLabelNodeIds) {
        if (nodeLabelItems.contains(nid) && nodeLabelItems[nid]) {
            nodeLabelItems[nid]->setVisible(true);
        }
    }
    hiddenLabelNodeIds.clear();

    // 停止并删除旧的出现动画
    for (auto &a : hoverAnims) {
        if (a) {
            a->stop();
            a->deleteLater();
        }
    }
    hoverAnims.clear();

    // 为每个气泡创建淡出动画
    for (auto* it : hoverItems) {
        if (!it) continue;
        HoverBubble* hb = qgraphicsitem_cast<HoverBubble*>(it);
        if (!hb) {
            // 不是HoverBubble，直接删除
            qDebug() << " - removing non-bubble item" << it;
            scene->removeItem(it);
            delete it;
            continue;
        }

        // 创建淡出+缩小动画
        auto* a_op = new QPropertyAnimation(hb, "opacity", this);
        a_op->setDuration(120);
        a_op->setStartValue(hb->opacity());
        a_op->setEndValue(0.0);
        a_op->setEasingCurve(QEasingCurve::InCubic);

        auto* a_sc = new QPropertyAnimation(hb, "bubbleScale", this);
        a_sc->setDuration(120);
        a_sc->setStartValue(hb->bubbleScale());
        a_sc->setEndValue(0.92);
        a_sc->setEasingCurve(QEasingCurve::InCubic);

        auto* group = new QParallelAnimationGroup(this);
        group->addAnimation(a_op);
        group->addAnimation(a_sc);
        
        // 动画完成后删除气泡
        connect(group, &QAbstractAnimation::finished, this, [this, hb, group](){
            qDebug() << " - fade-out finished, removing item" << hb;
            if (hb && scene) {
                scene->removeItem(hb);
                delete hb;
            }
            group->deleteLater();
        });
        
        group->start();
    }
    
    hoverItems.clear();
    this->viewport()->update();
}

void MapWidget::pauseHoverAnimations()
{
    // pause all running hover animations
    for (auto &a : hoverAnims) {
        if (a && a.data()) {
            QAbstractAnimation* anim = a.data();
            if (anim->state() == QAbstractAnimation::Running) {
                anim->pause();
            }
        }
    }
}

void MapWidget::resumeHoverAnimations()
{
    // resume paused hover animations
    for (auto &a : hoverAnims) {
        if (a && a.data()) {
            QAbstractAnimation* anim = a.data();
            if (anim->state() == QAbstractAnimation::Paused) {
                anim->resume();
            }
        }
    }
}
void MapWidget::startHoverAppearAnimation()
{
    qDebug() << "startHoverAppearAnimation() creating property animations for" << hoverItems.size() << "items";
    for (auto* it : hoverItems) {
        if (!it) continue;
        HoverBubble* hb = qgraphicsitem_cast<HoverBubble*>(it);
        if (!hb) continue;

        // initial state
        hb->setOpacity(0.0);
        hb->setBubbleScale(0.96);

        // opacity animation
        auto* a_op = new QPropertyAnimation(hb, "opacity", this);
        a_op->setDuration(140);
        a_op->setStartValue(0.0);
        a_op->setEndValue(1.0);
        a_op->setEasingCurve(QEasingCurve::OutCubic);

        // scale animation (custom property)
        auto* a_sc = new QPropertyAnimation(hb, "bubbleScale", this);
        a_sc->setDuration(140);
        a_sc->setStartValue(0.96);
        a_sc->setEndValue(1.0);
        a_sc->setEasingCurve(QEasingCurve::OutCubic);

        auto* group = new QParallelAnimationGroup(this);
        group->addAnimation(a_op);
        group->addAnimation(a_sc);
        connect(group, &QAbstractAnimation::finished, group, &QObject::deleteLater);

        hoverAnims.push_back(QPointer<QAbstractAnimation>(group));
        group->start();
    }
}

QColor MapWidget::nodeBaseColor(const Node& node) const
{
    Q_UNUSED(node);
    // 如需按类型/分类定色，可在此拓展映射；当前与 drawMap 一致
    return QColor("#2C3E50");
}

QColor MapWidget::edgeBaseColor(const Edge& edge) const
{
    Q_UNUSED(edge);
    // 如需按类型/属性定色，可在此拓展映射；当前与 drawMap 一致
    return QColor(200,200,200);
}

void MapWidget::showNodeHoverBubble(const Node& node)
{
    // create a single HoverBubble for the node
    const QColor basePenColor = nodeBaseColor(node);
    const QColor bubbleColor = withAlpha(basePenColor, 64);

    HoverBubble* hb = new HoverBubble();
    hb->setIsEdge(false);
    QString name = node.name.isEmpty() ? QString("节点 %1").arg(node.id) : node.name;
    QString desc = node.description;
    hb->setBaseColor(bubbleColor);
    hb->setContent(name, desc);
    hb->setZValue(100);
    hb->setCenterAt(QPointF(node.x, node.y));

    quint64 uid = hoverUidCounter++;
    hb->setData(1000, QVariant::fromValue((qulonglong)uid));
    scene->addItem(hb);
    hoverItems.push_back(hb);

    // 隐藏原始标签
    if (nodeLabelItems.contains(node.id) && nodeLabelItems[node.id]) {
        nodeLabelItems[node.id]->setVisible(false);
        hiddenLabelNodeIds.push_back(node.id);
    }

    startHoverAppearAnimation();
}

void MapWidget::showEdgeHoverBubble(const Edge& edge, const QPointF& closestPoint)
{
    // 原路径颜色（参考 drawMap 或按类型映射）
    const QColor baseEdgeColor = edgeBaseColor(edge);
    const QColor bubbleColor = withAlpha(baseEdgeColor, 64); // 约 75% 透明度

    // 端点
    QMap<int, Node> nodeMap; for (const auto& n : cachedNodes) nodeMap.insert(n.id, n);
    if (!nodeMap.contains(edge.u) || !nodeMap.contains(edge.v)) return;
    const auto& a = nodeMap[edge.u];
    const auto& b = nodeMap[edge.v];

    // 将膨胀线段并入 HoverBubble（在 HoverBubble 内绘制），并用相同 uid 管理
    quint64 uid = hoverUidCounter++;

    // 隐藏边两端的原始节点标签（避免重叠干扰）
    for (int nid : {edge.u, edge.v}) {
        if (nodeLabelItems.contains(nid) && nodeLabelItems[nid]) {
            nodeLabelItems[nid]->setVisible(false);
            hiddenLabelNodeIds.push_back(nid);
        }
    }

    // 文字内容
    QString name = edge.name.trimmed();
    if (name.isEmpty()) {
        // fallback：使用两端点名称
        QString au = nodeMap[edge.u].name; QString bv = nodeMap[edge.v].name;
        name = au.isEmpty() || bv.isEmpty() ? QString("%1-%2").arg(edge.u).arg(edge.v) : (au + " - " + bv);
    }
    QString desc = edge.description;

    // create HoverBubble for edge text/box at midpoint
    QPointF A(a.x, a.y), B(b.x, b.y);
    QPointF M((A.x()+B.x())/2.0, (A.y()+B.y())/2.0);
    double dx = B.x()-A.x();
    double dy = B.y()-A.y();
    double angle = std::atan2(dy, dx) * 180.0 / M_PI; // degrees

    HoverBubble* hb = new HoverBubble();
    hb->setIsEdge(true);
    hb->setBaseColor(bubbleColor);
    hb->setContent(name, desc);
    hb->setAngle(angle);
    hb->setEdgeLine(A, B);
    hb->setZValue(101);
    hb->setData(1000, QVariant::fromValue((qulonglong)uid));
    scene->addItem(hb);
    hoverItems.push_back(hb);

    startHoverAppearAnimation();
}

