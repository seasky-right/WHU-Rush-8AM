#include "MapWidget.h"
#include <QtWidgets/QGraphicsEllipseItem>
#include <QtWidgets/QGraphicsLineItem>
#include <QtWidgets/QGraphicsTextItem>
#include "HoverBubble.h"
#include <QtGui/QMouseEvent>
#include <QtCore/QDebug>
#include <cmath>
#include <QtWidgets/QScrollBar>
#include <QtCore/QDateTime>
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
    
    // 禁用默认拖拽，由我们在代码中手动接管中键
    this->setDragMode(QGraphicsView::NoDrag);
    
    // 初始化动画计时器
    animationTimer = new QTimer(this);
    connect(animationTimer, &QTimer::timeout, this, &MapWidget::onAnimationTick);

    // Hover 恢复计时器
    hoverResumeTimer = new QTimer(this);
    hoverResumeTimer->setSingleShot(true);
    connect(hoverResumeTimer, &QTimer::timeout, this, &MapWidget::resumeHoverAnimations);
}

void MapWidget::drawMap(const QVector<Node>& nodes, const QVector<Edge>& edges)
{
    // 清理场景 (保留背景)
    auto items = scene->items();
    for (auto it : items) {
        if (it != backgroundItem && it->zValue() > -50) {
            scene->removeItem(it);
            delete it;
        }
    }
    cachedNodes = nodes;
    cachedEdges = edges;
    nodeLabelItems.clear();

    QMap<int, Node> nodeMap;
    for(const auto& node : nodes) nodeMap.insert(node.id, node);

    // --- 1. 绘制连边 ---
    for(const auto& edge : edges) {
        if(nodeMap.contains(edge.u) && nodeMap.contains(edge.v)) {
            Node start = nodeMap[edge.u];
            Node end = nodeMap[edge.v];
            QPen edgePen = edgePenForType(edge.type);
            auto line = scene->addLine(start.x, start.y, end.x, end.y, edgePen);
            line->setZValue(10);
        }
    }

    // --- 2. 绘制节点 ---
    QPen nodePen(QColor("#2C3E50")); nodePen.setWidth(2);
    QBrush visibleBrush(QColor("white"));
    QBrush ghostBrush(QColor(180, 180, 200, 160));
    QPen ghostPen(QColor(140, 140, 160));

    for(const auto& node : nodes) {
        bool isGhost = (node.type == NodeType::Ghost);
        
        // 逻辑：如果是幽灵节点，且不在编辑模式，则不绘制
        if (isGhost && currentMode == EditMode::None) continue;
        
        double r = isGhost ? 5.0 : 8.0;
        
        // 连边模式高亮选中点
        QBrush b = (node.id == connectFirstNodeId) ? QBrush(Qt::yellow) : (isGhost ? ghostBrush : visibleBrush);
        
        auto item = scene->addEllipse(node.x - r, node.y - r, 2*r, 2*r, isGhost ? ghostPen : nodePen, b);
        item->setZValue(isGhost ? 9 : 11); // 可见节点层级更高
        item->setData(0, node.id);

        if (!node.name.isEmpty()) {
            auto text = scene->addText(node.name);
            QRectF bd = text->boundingRect();
            text->setPos(node.x - bd.width()/2.0, node.y + r + 2);
            text->setDefaultTextColor(QColor("#555"));
            text->setZValue(11);
            nodeLabelItems.insert(node.id, text);
        }
    }
    
    // 恢复：如果不强制 fitInView，至少保证 SceneRect 包含所有节点（如果没背景图）
    if (!backgroundItem) {
        double minX=1e9, minY=1e9, maxX=-1e9, maxY=-1e9;
        for(const auto& n: nodes) {
            minX = std::min(minX, n.x); minY = std::min(minY, n.y);
            maxX = std::max(maxX, n.x); maxY = std::max(maxY, n.y);
        }
        if(minX < maxX) scene->setSceneRect(minX-50, minY-50, maxX-minX+100, maxY-minY+100);
    }
}

void MapWidget::setEditMode(EditMode mode)
{
    currentMode = mode;
    connectFirstNodeId = -1; 
    clearPathHighlight(); // 切换模式时清除路径高亮
    clearHoverItems();    // 清除气泡
    drawMap(cachedNodes, cachedEdges); 
}

void MapWidget::setBackgroundImage(const QString& path)
{
    QPixmap px(path);
    if (px.isNull()) return;
    if (backgroundItem) { scene->removeItem(backgroundItem); delete backgroundItem; }
    
    scene->setSceneRect(0, 0, px.width(), px.height());
    backgroundItem = scene->addPixmap(px);
    backgroundItem->setZValue(-100);
    backgroundItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
}

// ---------------------------------------------------------
//   交互核心：融合 编辑器(Edit) 与 导航仪(View)
// ---------------------------------------------------------

void MapWidget::mousePressEvent(QMouseEvent *event)
{
    QPointF scenePos = mapToScene(event->pos());

    // 1. 中键平移 (通用)
    if (event->button() == Qt::MiddleButton) {
        isMiddlePanning = true;
        lastPanPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    // 2. 右键逻辑
    if (event->button() == Qt::RightButton) {
        if (currentMode == EditMode::None) {
            // [View Mode] 设置终点
            int hitId = findNodeAt(scenePos);
            if (hitId != -1) {
                QString name; 
                for(const auto&n:cachedNodes) if(n.id==hitId) name=n.name;
                emit nodeClicked(hitId, name, false); // false = Right Click (End)
            }
        } else {
            // [Edit Mode] 撤销 / 取消选择
            if (currentMode == EditMode::ConnectEdge && connectFirstNodeId != -1) {
                connectFirstNodeId = -1;
                drawMap(cachedNodes, cachedEdges);
            } else {
                emit undoRequested();
            }
        }
        event->accept();
        return;
    }

    // 3. 左键逻辑
    if (event->button() == Qt::LeftButton) {
        int hitId = findNodeAt(scenePos);

        // --- 拖拽逻辑 (仅 Edit Mode + Ctrl) ---
        if (currentMode != EditMode::None && (event->modifiers() & Qt::ControlModifier) && hitId != -1) {
            if (currentMode != EditMode::ConnectEdge) { // 连边模式不拖拽
                isNodeDragging = true;
                draggingNodeId = hitId;
                lastScenePos = scenePos;
                setCursor(Qt::SizeAllCursor);
                emit nodeEditClicked(hitId, true);
                event->accept();
                return;
            }
        }

        // --- 模式分发 ---
        if (currentMode == EditMode::None) {
            // [View Mode] 设置起点 / 气泡点击
            if (hitId != -1) {
                clearHoverItems(); // 点击时清理悬停
                QString name;
                for(const auto&n:cachedNodes) if(n.id==hitId) name=n.name;
                emit nodeClicked(hitId, name, true); // true = Left Click (Start)
            }
        }
        else if (currentMode == EditMode::ConnectEdge) {
            // [Edit Mode] 连边
            if (hitId != -1) {
                if (connectFirstNodeId == -1) {
                    connectFirstNodeId = hitId;
                    drawMap(cachedNodes, cachedEdges);
                } else if (hitId != connectFirstNodeId) {
                    emit edgeConnectionRequested(connectFirstNodeId, hitId);
                    connectFirstNodeId = -1;
                    drawMap(cachedNodes, cachedEdges);
                }
            }
        }
        else if (currentMode == EditMode::AddBuilding || currentMode == EditMode::AddGhost) {
            // [Edit Mode] 新建/属性
            if (hitId != -1) {
                emit nodeEditClicked(hitId, false);
            } else {
                emit emptySpaceClicked(scenePos.x(), scenePos.y());
            }
        }
        event->accept();
    }
}

void MapWidget::mouseMoveEvent(QMouseEvent *event)
{
    QPointF scenePos = mapToScene(event->pos());

    // 1. 中键平移
    if (isMiddlePanning) {
        QPoint delta = event->pos() - lastPanPos;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        lastPanPos = event->pos();
        event->accept();
        return;
    }

    // 2. 节点拖拽 (Edit Mode)
    if (isNodeDragging && draggingNodeId != -1) {
        double dx = scenePos.x() - lastScenePos.x();
        double dy = scenePos.y() - lastScenePos.y();
        
        // 简单查找原始坐标并累加
        for(auto& n : cachedNodes) {
            if(n.id == draggingNodeId) {
                emit nodeMoved(draggingNodeId, n.x + dx, n.y + dy);
                lastScenePos = scenePos;
                break;
            }
        }
        event->accept();
        return;
    }

    // 3. 悬停气泡 (View Mode 且 未按下鼠标)
    if (currentMode == EditMode::None && event->buttons() == Qt::NoButton) {
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
            event->accept(); return;
        }

        QPointF closest; int u=-1, v=-1;
        int edgeIdx = findEdgeAt(scenePos, closest, u, v);
        if (edgeIdx != -1) {
            if (hoveredEdgeIndex != edgeIdx || hoveredNodeId != -1) {
                clearHoverItems();
                hoveredEdgeIndex = edgeIdx;
                hoveredNodeId = -1;
                showEdgeHoverBubble(cachedEdges[edgeIdx], closest);
            }
            event->accept(); return;
        }

        if (hoveredNodeId != -1 || hoveredEdgeIndex != -1) {
            hoveredNodeId = -1; hoveredEdgeIndex = -1;
            clearHoverItems();
        }
    }

    QGraphicsView::mouseMoveEvent(event);
}

void MapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        isMiddlePanning = false;
        setCursor(Qt::ArrowCursor);
    }
    if (event->button() == Qt::LeftButton && isNodeDragging) {
        isNodeDragging = false;
        draggingNodeId = -1;
        setCursor(Qt::ArrowCursor);
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void MapWidget::wheelEvent(QWheelEvent *event)
{
    // 缩放逻辑
    const double scaleFactor = 1.15;
    if (event->angleDelta().y() > 0) {
        this->scale(scaleFactor, scaleFactor);
        currentScale *= scaleFactor;
    } else {
        this->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
        currentScale /= scaleFactor;
    }
}

// ---------------------------------------------------------
//   恢复：动效与气泡逻辑
// ---------------------------------------------------------

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

    // 绘制高亮底色 (半透明蓝)
    for (int i = 0; i < pathNodeIds.size() - 1; ++i) {
        int u = pathNodeIds[i];
        int v = pathNodeIds[i + 1];
        if (nodeMap.contains(u) && nodeMap.contains(v)) {
            const Node& a = nodeMap[u];
            const Node& b = nodeMap[v];
            QPen pen(QColor(100, 180, 240, 100)); // 较淡的底色
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
    // 移除旧生长线 (Z=20)
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

void MapWidget::clearPathHighlight()
{
    if (animationTimer->isActive()) animationTimer->stop();
    animationProgress = 0.0;
    currentPathNodeIds.clear();
    auto items = scene->items();
    for (auto it : items) {
        double z = it->zValue();
        if (z >= 15.0 && z <= 20.0) { scene->removeItem(it); delete it; }
    }
    viewport()->update();
}

// 气泡相关
void MapWidget::clearHoverItems() {
    if (hoverItems.isEmpty()) return;
    for (int nid : hiddenLabelNodeIds) if (nodeLabelItems.contains(nid) && nodeLabelItems[nid]) nodeLabelItems[nid]->setVisible(true);
    hiddenLabelNodeIds.clear();
    for (auto &a : hoverAnims) if(a) { a->stop(); a->deleteLater(); }
    hoverAnims.clear();
    for (auto* it : hoverItems) { if (it) { scene->removeItem(it); delete it; } }
    hoverItems.clear();
}

QColor MapWidget::withAlpha(const QColor& c, int alpha) { QColor t=c; t.setAlpha(alpha); return t; }

void MapWidget::showNodeHoverBubble(const Node& node) {
    const QColor basePenColor = QColor("#2C3E50");
    const QColor bubbleColor = withAlpha(basePenColor, 180); // 稍微不透明点
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
    const QColor bubbleColor = withAlpha(baseEdgeColor, 180);
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
        hb->setOpacity(0.0); hb->setBubbleScale(0.96);
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
void MapWidget::pauseHoverAnimations() { for(auto&a:hoverAnims) if(a && a->state()==QAbstractAnimation::Running) a->pause(); }
void MapWidget::resumeHoverAnimations() { for(auto&a:hoverAnims) if(a && a->state()==QAbstractAnimation::Paused) a->resume(); }
void MapWidget::resizeEvent(QResizeEvent *event) { QGraphicsView::resizeEvent(event); pauseHoverAnimations(); if(hoverResumeTimer) hoverResumeTimer->start(300); }
void MapWidget::leaveEvent(QEvent *event) { Q_UNUSED(event); clearHoverItems(); }

// 辅助
int MapWidget::findNodeAt(const QPointF& pos) {
    double threshold = 20.0;
    for(const auto& node : cachedNodes) {
        if (node.type == NodeType::Ghost && currentMode == EditMode::None) continue;
        double dx = pos.x() - node.x; double dy = pos.y() - node.y;
        if (dx*dx + dy*dy < threshold*threshold) return node.id;
    }
    return -1;
}

int MapWidget::findEdgeAt(const QPointF& pos, QPointF& closestPoint, int& outU, int& outV) {
    if (cachedEdges.isEmpty() || cachedNodes.isEmpty()) return -1;
    QMap<int, Node> nodeMap; for (const auto& n : cachedNodes) nodeMap.insert(n.id, n);
    int bestIdx = -1; double bestDist = 1e18; QPointF bestPt;
    int bu=-1, bv=-1; const double threshold = 10.0;
    for (int i = 0; i < cachedEdges.size(); ++i) {
        const auto& e = cachedEdges[i];
        if (!nodeMap.contains(e.u) || !nodeMap.contains(e.v)) continue;
        const auto& a = nodeMap[e.u]; const auto& b = nodeMap[e.v];
        QPointF p(pos.x(), pos.y()); QPointF A(a.x, a.y), B(b.x, b.y);
        QPointF AB = B - A; double ab2 = AB.x()*AB.x() + AB.y()*AB.y();
        if (ab2 <= 1e-6) continue;
        double t = ((p.x()-A.x())*AB.x() + (p.y()-A.y())*AB.y()) / ab2;
        t = std::max(0.0, std::min(1.0, t));
        QPointF proj(A.x()+AB.x()*t, A.y()+AB.y()*t);
        double dist2 = (p.x()-proj.x())*(p.x()-proj.x()) + (p.y()-proj.y())*(p.y()-proj.y());
        if (dist2 < bestDist) { bestDist = dist2; bestPt = proj; bu = e.u; bv = e.v; bestIdx = i; }
    }
    if (bestIdx != -1 && std::sqrt(bestDist) <= threshold) { closestPoint = bestPt; outU = bu; outV = bv; return bestIdx; }
    return -1;
}

QPen MapWidget::edgePenForType(EdgeType type) const {
    QColor c(200,200,200); int width = 3;
    switch (type) {
    case EdgeType::Normal: c = QColor(180,180,180); width = 3; break;
    case EdgeType::Main:   c = QColor(150,180,220); width = 4; break;
    case EdgeType::Path:   c = QColor(120,200,140); width = 3; break;
    case EdgeType::Indoor: c = QColor(220,160,120); width = 3; break;
    }
    QPen pen(c); pen.setWidth(width);
    pen.setJoinStyle(Qt::RoundJoin); pen.setCapStyle(Qt::RoundCap);
    return pen;
}

// Editor Stub
void MapWidget::clearEditTempItems() {}
void MapWidget::addEditVisualNode(int, const QString&, const QPointF&, int) {}