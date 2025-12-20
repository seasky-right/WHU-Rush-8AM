#include "MapWidget.h"
#include <QtWidgets/QGraphicsEllipseItem>
#include <QtWidgets/QGraphicsLineItem>
#include <QtWidgets/QGraphicsTextItem>
#include <QtWidgets/QGraphicsObject> // 必须包含
#include "HoverBubble.h"
#include <QtGui/QMouseEvent>
#include <QtCore/QDebug>
#include <cmath>
#include <QtWidgets/QScrollBar>
#include <QtCore/QDateTime>
#include <QtCore/QEasingCurve>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QParallelAnimationGroup>
#include <QtGui/QPainterPath>
#include <QtGui/QRadialGradient>
#include <QtWidgets/QGraphicsDropShadowEffect>
#include <limits>

// =============================================================
//  内部辅助类：必须继承自 QGraphicsObject 才能支持 QPropertyAnimation
// =============================================================

class HaloItem : public QGraphicsObject {
public:
    HaloItem(const QPointF& center, double radius, QGraphicsItem* parent = nullptr)
        : QGraphicsObject(parent), m_center(center), m_radius(radius) {
        setAcceptedMouseButtons(Qt::NoButton);
        setZValue(99); 
    }
    QRectF boundingRect() const override {
        return QRectF(m_center.x() - m_radius, m_center.y() - m_radius, m_radius*2, m_radius*2);
    }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        QRadialGradient grad(m_center, m_radius);
        grad.setColorAt(0.0, QColor(0, 122, 255, 60)); 
        grad.setColorAt(0.5, QColor(0, 122, 255, 30)); 
        grad.setColorAt(1.0, QColor(0, 122, 255, 0));  
        painter->setPen(Qt::NoPen);
        painter->setBrush(grad);
        painter->drawEllipse(m_center, m_radius, m_radius);
    }
private:
    QPointF m_center;
    double m_radius;
};

class GlowItem : public QGraphicsObject {
public:
    GlowItem(const QPointF& p1, const QPointF& p2, double width, QGraphicsItem* parent = nullptr)
        : QGraphicsObject(parent), m_p1(p1), m_p2(p2), m_width(width) {
        setAcceptedMouseButtons(Qt::NoButton);
        setZValue(99);
    }
    QRectF boundingRect() const override {
        double minx = std::min(m_p1.x(), m_p2.x()) - m_width;
        double miny = std::min(m_p1.y(), m_p2.y()) - m_width;
        double w = std::abs(m_p1.x()-m_p2.x()) + m_width*2;
        double h = std::abs(m_p1.y()-m_p2.y()) + m_width*2;
        return QRectF(minx, miny, w, h);
    }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        QPen glowPen(QColor(0, 122, 255, 40)); 
        glowPen.setWidth(m_width); 
        glowPen.setCapStyle(Qt::RoundCap);
        painter->setPen(glowPen);
        painter->drawLine(m_p1, m_p2);
    }
private:
    QPointF m_p1, m_p2;
    double m_width;
};

// =============================================================
//  MapWidget 实现
// =============================================================

MapWidget::MapWidget(QWidget *parent) : QGraphicsView(parent)
{
    scene = new QGraphicsScene(this);
    this->setScene(scene);
    
    this->setRenderHint(QPainter::Antialiasing);
    this->setRenderHint(QPainter::SmoothPixmapTransform);
    this->setRenderHint(QPainter::TextAntialiasing);
    
    this->setBackgroundBrush(QBrush(QColor("#F5F5F7")));
    
    this->setMouseTracking(true);
    this->setDragMode(QGraphicsView::NoDrag);
    
    animationTimer = new QTimer(this);
    connect(animationTimer, &QTimer::timeout, this, &MapWidget::onAnimationTick);

    hoverResumeTimer = new QTimer(this);
    hoverResumeTimer->setSingleShot(true);
    connect(hoverResumeTimer, &QTimer::timeout, this, &MapWidget::resumeHoverAnimations);
}

void MapWidget::setActiveEdge(int u, int v) {
    activeEdgeU = u;
    activeEdgeV = v;
    drawMap(cachedNodes, cachedEdges);
}

void MapWidget::stopHoverAnimations() {
    for (auto& a : hoverAnims) { 
        if (a) { 
            a->stop(); 
            delete a; 
        } 
    }
    hoverAnims.clear();
}

void MapWidget::drawMap(const QVector<Node>& nodes, const QVector<Edge>& edges)
{
    // 1. 停止动画 (必须在删除 Item 之前)
    if (animationTimer->isActive()) animationTimer->stop();
    stopHoverAnimations();

    // 2. 清理指针
    activeTrackItem = nullptr;
    activeGrowthItem = nullptr;
    currentPathNodeIds.clear();

    hoverItems.clear();
    dyingItems.clear();
    editTempItems.clear();    
    nodeLabelItems.clear();
    hiddenLabelNodeIds.clear();
    
    // 3. 物理清理场景
    auto items = scene->items();
    for (auto it : items) {
        if (it != backgroundItem && it->zValue() > -100) {
            scene->removeItem(it);
            delete it; 
        }
    }

    cachedNodes = nodes;
    cachedEdges = edges;

    QMap<int, Node> nodeMap;
    for(const auto& node : nodes) nodeMap.insert(node.id, node);

    // 绘制边
    QPen edgePen(QColor("#C7C7CC")); 
    edgePen.setWidth(2);
    edgePen.setCapStyle(Qt::RoundCap);

    QPen activeEdgePen(QColor("#007AFF"));
    activeEdgePen.setWidth(4);
    activeEdgePen.setCapStyle(Qt::RoundCap);

    for(const auto& edge : edges) {
        if(nodeMap.contains(edge.u) && nodeMap.contains(edge.v)) {
            Node start = nodeMap[edge.u];
            Node end = nodeMap[edge.v];
            
            bool isActiveEdge = false;
            if (activeEdgeU != -1 && activeEdgeV != -1) {
                if ((edge.u == activeEdgeU && edge.v == activeEdgeV) || 
                    (edge.u == activeEdgeV && edge.v == activeEdgeU)) {
                    isActiveEdge = true;
                }
            }
            auto line = scene->addLine(start.x, start.y, end.x, end.y, isActiveEdge ? activeEdgePen : edgePen);
            line->setZValue(isActiveEdge ? 5 : 0); 
        }
    }

    // 连线预览
    if (currentMode == EditMode::ConnectEdge && connectFirstNodeId != -1 && hoveredNodeId != -1 && connectFirstNodeId != hoveredNodeId) {
        if (nodeMap.contains(connectFirstNodeId) && nodeMap.contains(hoveredNodeId)) {
            Node start = nodeMap[connectFirstNodeId];
            Node end = nodeMap[hoveredNodeId];
            QPen dashPen(QColor("#007AFF")); 
            dashPen.setWidth(2);
            dashPen.setStyle(Qt::DashLine);
            auto previewLine = scene->addLine(start.x, start.y, end.x, end.y, dashPen);
            previewLine->setZValue(1); 
        }
    }

    // 绘制节点
    QColor colorNormal("#8E8E93");     
    QColor colorActive("#007AFF");     
    QColor colorGhost("#E5E5EA");      
    
    QFont nameFont("PingFang SC", 9);  
    if (!QFontInfo(nameFont).exactMatch()) nameFont.setFamily("Microsoft YaHei");
    nameFont.setWeight(QFont::DemiBold);

    for(const auto& node : nodes) {
        bool isGhost = (node.type == NodeType::Ghost);
        if (isGhost && currentMode == EditMode::None) continue;
        
        bool isStart = (currentMode == EditMode::ConnectEdge && node.id == connectFirstNodeId);
        bool isTarget = (currentMode == EditMode::ConnectEdge && connectFirstNodeId != -1 && node.id == hoveredNodeId);
        bool isActiveEndpoint = (activeEdgeU != -1 && (node.id == activeEdgeU || node.id == activeEdgeV));
        bool isDragging = (isNodeDragging && node.id == draggingNodeId);
        bool isHighlight = isStart || isTarget || isActiveEndpoint || isDragging;

        double r = isGhost ? 4.0 : 6.0;
        int zValue = isGhost ? 5 : 10;
        QColor fillColor = isGhost ? colorGhost : colorNormal;
        QColor strokeColor = isGhost ? QColor(200,200,200) : Qt::white;
        double strokeWidth = 2.0;

        if (isHighlight) {
            double haloR = r + 6.0;
            QColor haloColor = colorActive; 
            haloColor.setAlpha(80); 
            auto halo = scene->addEllipse(node.x - haloR, node.y - haloR, haloR*2, haloR*2, Qt::NoPen, QBrush(haloColor));
            halo->setZValue(zValue - 1); 
            
            fillColor = colorActive;
            strokeColor = Qt::white; 
            strokeWidth = 3.0;      
            r += 1.0;               
            zValue = 100;           
        } 

        QPen p(strokeColor); 
        p.setWidthF(strokeWidth);
        QBrush b(fillColor);
        auto item = scene->addEllipse(node.x - r, node.y - r, 2*r, 2*r, p, b);
        item->setZValue(zValue); 
        item->setData(0, node.id);
        
        if (!isGhost || isHighlight) {
            auto* effect = new QGraphicsDropShadowEffect();
            effect->setBlurRadius(isHighlight ? 12 : 6);
            effect->setOffset(0, 2);
            effect->setColor(QColor(0, 0, 0, isHighlight ? 80 : 30));
            item->setGraphicsEffect(effect);
        }

        if (!isGhost && !node.name.isEmpty()) {
            auto text = scene->addText(node.name);
            text->setFont(nameFont);
            text->setDefaultTextColor(isHighlight ? colorActive : QColor("#1C1C1E")); 
            QRectF bd = text->boundingRect();
            text->setPos(node.x - bd.width()/2.0, node.y + r + 4);
            text->setZValue(zValue); 
            text->setAcceptedMouseButtons(Qt::NoButton); 
            
            if (isHighlight) {
                QFont f = text->font(); f.setBold(true); text->setFont(f);
            }
            nodeLabelItems.insert(node.id, text);
        }
    }
    
    if (!backgroundItem) {
        double minX=1e9, minY=1e9, maxX=-1e9, maxY=-1e9;
        for(const auto& n: nodes) {
            minX = std::min(minX, n.x); minY = std::min(minY, n.y);
            maxX = std::max(maxX, n.x); maxY = std::max(maxY, n.y);
        }
        if(minX < maxX) scene->setSceneRect(minX-50, minY-50, maxX-minX+100, maxY-minY+100);
    }
}

// ---------------------------------------------------------
//   悬停气泡与动画 (修复版)
// ---------------------------------------------------------

void MapWidget::clearHoverItems() {
    for (int nid : hiddenLabelNodeIds) if (nodeLabelItems.contains(nid) && nodeLabelItems[nid]) nodeLabelItems[nid]->setVisible(true);
    hiddenLabelNodeIds.clear();
    stopHoverAnimations();
    for (auto* it : hoverItems) { if (it) { scene->removeItem(it); delete it; } }
    hoverItems.clear();
    killDyingItems();
}

void MapWidget::killDyingItems() {
    for (auto* it : dyingItems) {
        if (it) { scene->removeItem(it); delete it; }
    }
    dyingItems.clear();
}

void MapWidget::fadeOutHoverItems() {
    if (hoverItems.isEmpty()) return;

    for (int nid : hiddenLabelNodeIds) if (nodeLabelItems.contains(nid) && nodeLabelItems[nid]) nodeLabelItems[nid]->setVisible(true);
    hiddenLabelNodeIds.clear();

    stopHoverAnimations(); // 停止旧动画

    dyingItems.append(hoverItems);
    hoverItems.clear();

    auto* group = new QParallelAnimationGroup(this);
    for (auto* it : dyingItems) {
        if (!it) continue;

        // 【修复编译错误】: 只有当 item 是 QGraphicsObject 时才能使用 QPropertyAnimation
        // HaloItem, GlowItem, HoverBubble 都是 QGraphicsObject
        QGraphicsObject* obj = dynamic_cast<QGraphicsObject*>(it);
        if (obj) {
            auto* a_op = new QPropertyAnimation(obj, "opacity", this);
            a_op->setDuration(200);
            a_op->setStartValue(obj->opacity());
            a_op->setEndValue(0.0);
            group->addAnimation(a_op);

            // 仅气泡缩放
            HoverBubble* hb = qobject_cast<HoverBubble*>(obj);
            if (hb) {
                auto* a_sc = new QPropertyAnimation(hb, "bubbleScale", this);
                a_sc->setDuration(200);
                a_sc->setStartValue(hb->bubbleScale());
                a_sc->setEndValue(0.95); 
                group->addAnimation(a_sc);
            }
        } else {
            // 如果不是 QObject (理论上不应发生)，直接移除
            scene->removeItem(it);
            delete it;
        }
    }
    
    connect(group, &QAbstractAnimation::finished, this, [this, group]() {
        killDyingItems();
        group->deleteLater();
    });
    
    hoverAnims.push_back(QPointer<QAbstractAnimation>(group));
    group->start();
}

QColor MapWidget::withAlpha(const QColor& c, int alpha) { QColor t=c; t.setAlpha(alpha); return t; }

void MapWidget::startHoverAppearAnimation() {
    auto* group = new QParallelAnimationGroup(this);

    for (auto* it : hoverItems) {
        if (!it) continue;
        it->setOpacity(0.0);

        // 【修复编译错误】: 强制转换为 QGraphicsObject*
        QGraphicsObject* obj = dynamic_cast<QGraphicsObject*>(it);
        if (obj) {
            auto* a_op = new QPropertyAnimation(obj, "opacity", this);
            a_op->setDuration(180);
            a_op->setStartValue(0.0);
            a_op->setEndValue(1.0); 
            group->addAnimation(a_op);

            HoverBubble* hb = qobject_cast<HoverBubble*>(obj);
            if (hb) {
                hb->setBubbleScale(0.92); 
                auto* a_sc = new QPropertyAnimation(hb, "bubbleScale", this);
                a_sc->setDuration(300);
                a_sc->setStartValue(0.92);
                a_sc->setEndValue(1.0);
                a_sc->setEasingCurve(QEasingCurve::OutBack); 
                group->addAnimation(a_sc);
            }
        }
    }
    
    connect(group, &QAbstractAnimation::finished, group, &QObject::deleteLater);
    hoverAnims.push_back(QPointer<QAbstractAnimation>(group));
    group->start();
}

void MapWidget::showNodeHoverBubble(const Node& node) {
    stopHoverAnimations();
    killDyingItems();
    clearHoverItems(); 
    
    // HaloItem 是 QGraphicsObject
    auto* halo = new HaloItem(QPointF(node.x, node.y), 25.0);
    scene->addItem(halo);
    hoverItems.push_back(halo);

    const QColor bubbleColor(255, 255, 255, 215); 
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
    stopHoverAnimations();
    killDyingItems();
    clearHoverItems();

    QMap<int, Node> nodeMap; for (const auto& n : cachedNodes) nodeMap.insert(n.id, n);
    if (!nodeMap.contains(edge.u) || !nodeMap.contains(edge.v)) return;
    Node u = nodeMap[edge.u]; Node v = nodeMap[edge.v];
    
    // GlowItem 是 QGraphicsObject
    auto* glow = new GlowItem(QPointF(u.x, u.y), QPointF(v.x, v.y), 12.0);
    scene->addItem(glow);
    hoverItems.push_back(glow);

    const QColor baseEdgeColor = edgePenForType(edge.type).color();
    QColor bubbleColor = baseEdgeColor.lighter(170); 
    bubbleColor.setAlpha(225); 
    
    HoverBubble* hb = new HoverBubble();
    hb->setIsEdge(true);
    hb->setBaseColor(bubbleColor);
    hb->setContent(edge.name.isEmpty() ? QString("%1-%2").arg(edge.u).arg(edge.v) : edge.name, edge.description);
    QPointF A(u.x, u.y), B(v.x, v.y);
    hb->setEdgeLine(A, B);
    hb->setZValue(100);
    scene->addItem(hb);
    hoverItems.push_back(hb);
    
    startHoverAppearAnimation();
}

// ---------------------------------------------------------
//   常规函数
// ---------------------------------------------------------

void MapWidget::setEditMode(EditMode mode)
{
    currentMode = mode;
    connectFirstNodeId = -1; 
    setActiveEdge(-1, -1); 
    clearPathHighlight();
    fadeOutHoverItems(); 
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

void MapWidget::mousePressEvent(QMouseEvent *event)
{
    QPointF scenePos = mapToScene(event->pos());

    if (event->button() == Qt::MiddleButton) {
        isMiddlePanning = true;
        lastPanPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton) {
        if (currentMode == EditMode::None) {
            int hitId = findNodeAt(scenePos);
            if (hitId != -1) {
                QString name; 
                for(const auto&n:cachedNodes) if(n.id==hitId) name=n.name;
                fadeOutHoverItems();
                emit nodeClicked(hitId, name, false); 
            }
        } else {
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

    if (event->button() == Qt::LeftButton) {
        int hitId = findNodeAt(scenePos);

        if (currentMode != EditMode::None && (event->modifiers() & Qt::ControlModifier) && hitId != -1) {
            if (currentMode != EditMode::ConnectEdge) { 
                isNodeDragging = true;
                draggingNodeId = hitId;
                lastScenePos = scenePos;
                setCursor(Qt::SizeAllCursor);
                emit nodeEditClicked(hitId, true);
                drawMap(cachedNodes, cachedEdges); 
                event->accept();
                return;
            }
        }

        if (currentMode == EditMode::None) {
            if (hitId != -1) {
                fadeOutHoverItems(); 
                QString name;
                for(const auto&n:cachedNodes) if(n.id==hitId) name=n.name;
                emit nodeClicked(hitId, name, true); 
            }
        }
        else if (currentMode == EditMode::ConnectEdge) {
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

    if (isMiddlePanning) {
        QPoint delta = event->pos() - lastPanPos;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        lastPanPos = event->pos();
        event->accept();
        return;
    }

    if (isNodeDragging && draggingNodeId != -1) {
        double dx = scenePos.x() - lastScenePos.x();
        double dy = scenePos.y() - lastScenePos.y();
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

    int hitNode = findNodeAt(scenePos);
    
    if (currentMode == EditMode::ConnectEdge) {
        if (hitNode != hoveredNodeId) {
            hoveredNodeId = hitNode;
            drawMap(cachedNodes, cachedEdges); 
        }
        event->accept();
        return;
    }

    if (currentMode == EditMode::None && event->buttons() == Qt::NoButton) {
        if (hitNode != -1) {
            if (hoveredNodeId != hitNode || hoveredEdgeIndex != -1) {
                stopHoverAnimations();
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
                stopHoverAnimations();
                clearHoverItems();
                hoveredEdgeIndex = edgeIdx;
                hoveredNodeId = -1;
                showEdgeHoverBubble(cachedEdges[edgeIdx], closest);
            }
            event->accept(); return;
        }

        if (hoveredNodeId != -1 || hoveredEdgeIndex != -1) {
            hoveredNodeId = -1; hoveredEdgeIndex = -1;
            fadeOutHoverItems(); 
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
        drawMap(cachedNodes, cachedEdges); 
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void MapWidget::wheelEvent(QWheelEvent *event)
{
    double viewW = viewport()->width();
    double viewH = viewport()->height();
    double sceneW = scene->width();
    double sceneH = scene->height();
    
    if (sceneW <= 0 || sceneH <= 0) {
        QGraphicsView::wheelEvent(event);
        return;
    }

    double minScaleX = viewW / sceneW;
    double minScaleY = viewH / sceneH;
    double minScale = std::max(minScaleX, minScaleY);
    
    double currentTransformScale = transform().m11();
    const double scaleFactor = 1.15;
    double newScale = currentTransformScale;

    if (event->angleDelta().y() > 0) {
        this->scale(scaleFactor, scaleFactor);
    } else {
        newScale /= scaleFactor;
        if (newScale < minScale) {
            double factor = minScale / currentTransformScale;
            if (factor < 1.0) this->scale(factor, factor);
        } else {
            this->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
        }
    }
    event->accept();
}

void MapWidget::highlightPath(const QVector<int>& pathNodeIds, double animationDuration)
{
    if (currentPathNodeIds == pathNodeIds && animationTimer->isActive()) {
        return;
    }

    clearPathHighlight();
    
    if (pathNodeIds.size() < 2) return;

    currentPathNodeIds = pathNodeIds;
    animationDurationMs = animationDuration * 1000.0;
    animationProgress = 0.0;
    animationStartTime = QDateTime::currentMSecsSinceEpoch();

    QPainterPath fullPath;
    QMap<int, Node> nodeMap; 
    for(const auto& n : cachedNodes) nodeMap.insert(n.id, n);

    Node start = nodeMap[pathNodeIds[0]];
    fullPath.moveTo(start.x, start.y);
    for(int i=1; i<pathNodeIds.size(); ++i) {
        Node n = nodeMap[pathNodeIds[i]];
        fullPath.lineTo(n.x, n.y);
    }
    
    QPen trackPen(QColor(0, 122, 255, 40)); 
    trackPen.setWidth(8); 
    trackPen.setCapStyle(Qt::RoundCap);
    trackPen.setJoinStyle(Qt::RoundJoin);
    activeTrackItem = scene->addPath(fullPath, trackPen);
    activeTrackItem->setZValue(15);

    animationTimer->start(16);
}

void MapWidget::onAnimationTick()
{
    if (currentPathNodeIds.isEmpty()) { animationTimer->stop(); return; }
    
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - animationStartTime;
    animationProgress = (double)elapsed / animationDurationMs;

    if (animationProgress >= 1.0) {
        animationProgress = 1.0;
        animationTimer->stop();
    }
    drawPathGrowthAnimation();
}

void MapWidget::drawPathGrowthAnimation()
{
    if (activeGrowthItem) {
        scene->removeItem(activeGrowthItem);
        delete activeGrowthItem;
        activeGrowthItem = nullptr;
    }

    QMap<int, Node> nodeMap;
    for(const auto& n : cachedNodes) nodeMap.insert(n.id, n);

    QPainterPath currPath;
    Node n0 = nodeMap[currentPathNodeIds[0]];
    currPath.moveTo(n0.x, n0.y);

    double totalSegs = currentPathNodeIds.size() - 1;
    double currentPos = animationProgress * totalSegs;
    int segIndex = (int)currentPos;
    double segLocalProgress = currentPos - segIndex;

    QPointF tipPos(n0.x, n0.y);

    for (int i = 0; i < segIndex && i < totalSegs; ++i) {
        Node n = nodeMap[currentPathNodeIds[i+1]];
        currPath.lineTo(n.x, n.y);
        tipPos = QPointF(n.x, n.y);
    }

    if (segIndex < totalSegs) {
        Node start = nodeMap[currentPathNodeIds[segIndex]];
        Node end   = nodeMap[currentPathNodeIds[segIndex+1]];
        double dx = end.x - start.x;
        double dy = end.y - start.y;
        tipPos = QPointF(start.x + dx*segLocalProgress, start.y + dy*segLocalProgress);
        currPath.lineTo(tipPos);
    }

    QPen growPen(QColor("#007AFF"));
    growPen.setWidth(5);
    growPen.setCapStyle(Qt::RoundCap);
    growPen.setJoinStyle(Qt::RoundJoin);
    
    activeGrowthItem = scene->addPath(currPath, growPen);
    activeGrowthItem->setZValue(20);
}

void MapWidget::clearPathHighlight()
{
    if (animationTimer->isActive()) animationTimer->stop();
    animationProgress = 0.0;
    currentPathNodeIds.clear();
    
    if (activeTrackItem) {
        scene->removeItem(activeTrackItem);
        delete activeTrackItem;
        activeTrackItem = nullptr;
    }
    if (activeGrowthItem) {
        scene->removeItem(activeGrowthItem);
        delete activeGrowthItem;
        activeGrowthItem = nullptr;
    }
    
    this->viewport()->update();
}

void MapWidget::pauseHoverAnimations() {
    for (auto &a : hoverAnims) {
        if (a && a->state() == QAbstractAnimation::Running) {
            a->pause();
        }
    }
}

void MapWidget::resumeHoverAnimations() {
    for (auto &a : hoverAnims) {
        if (a && a->state() == QAbstractAnimation::Paused) {
            a->resume();
        }
    }
}

void MapWidget::resizeEvent(QResizeEvent *event) {
    QGraphicsView::resizeEvent(event);
    pauseHoverAnimations();
    if (hoverResumeTimer) {
        hoverResumeTimer->start(300);
    }
}

void MapWidget::leaveEvent(QEvent *event) {
    Q_UNUSED(event);
    fadeOutHoverItems();
}

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
    case EdgeType::Normal: c = QColor(220,220,220); width = 3; break;
    case EdgeType::Main:   c = QColor(180,200,230); width = 4; break;
    case EdgeType::Path:   c = QColor(180,220,180); width = 2; break;
    case EdgeType::Indoor: c = QColor(230,200,180); width = 2; break;
    }
    QPen pen(c); pen.setWidth(width);
    pen.setJoinStyle(Qt::RoundJoin); pen.setCapStyle(Qt::RoundCap);
    return pen;
}

void MapWidget::addEditVisualNode(int id, const QString& name, const QPointF& pos, int typeInt) {
    NodeType type = (typeInt == 9) ? NodeType::Ghost : NodeType::Visible;
    QPen p(type == NodeType::Ghost ? QColor(140,140,160) : QColor("#007AFF"));
    QBrush b(type == NodeType::Ghost ? QColor(230,230,240) : QColor("#007AFF"));
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

void MapWidget::clearEditTempItems() {
    for (auto* it : editTempItems) {
        if (it) { scene->removeItem(it); delete it; }
    }
    editTempItems.clear();
}