#include "MapWidget.h"
#include <QtWidgets/QGraphicsEllipseItem>
#include <QtWidgets/QGraphicsLineItem>
#include <QtWidgets/QGraphicsTextItem>
#include <QtWidgets/QGraphicsObject> 
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
#include <algorithm>

// =========================================================
//  辅助函数与类
// =========================================================

static long long makeEdgeKey(int u, int v) {
    int minId = std::min(u, v);
    int maxId = std::max(u, v);
    return (static_cast<long long>(minId) << 32) | maxId;
}

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

// =========================================================
//  MapWidget 实现
// =========================================================

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
    
    QString scrollStyle = 
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 0px; }"
        "QScrollBar::handle:vertical { background: #C1C1C5; min-height: 20px; border-radius: 4px; }"
        "QScrollBar::handle:vertical:hover { background: #8E8E93; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
        "QScrollBar:horizontal { background: transparent; height: 8px; margin: 0px; }"
        "QScrollBar::handle:horizontal { background: #C1C1C5; min-width: 20px; border-radius: 4px; }"
        "QScrollBar::handle:horizontal:hover { background: #8E8E93; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }";

    this->verticalScrollBar()->setStyleSheet(scrollStyle);
    this->horizontalScrollBar()->setStyleSheet(scrollStyle);
    
    animationTimer = new QTimer(this);
    connect(animationTimer, &QTimer::timeout, this, &MapWidget::onAnimationTick);

    hoverResumeTimer = new QTimer(this);
    hoverResumeTimer->setSingleShot(true);
    connect(hoverResumeTimer, &QTimer::timeout, this, &MapWidget::resumeHoverAnimations);

    auto updateWeatherPos = [this]() {
        if (weatherOverlay) {
            QPointF sceneTopLeft = mapToScene(0, 0);
            weatherOverlay->setPos(sceneTopLeft);
            if (viewport()) {
                weatherOverlay->setOverlayRect(QRectF(0, 0, viewport()->width(), viewport()->height()));
            }
        }
    };
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, updateWeatherPos);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, updateWeatherPos);
}

void MapWidget::setActiveEdge(int u, int v) {
    activeEdgeU = u;
    activeEdgeV = v;
    drawMap(cachedNodes, cachedEdges);
}

void MapWidget::stopHoverAnimations() {
    for (auto& a : hoverAnims) { if (a) { a->stop(); delete a; } }
    hoverAnims.clear();
}

void MapWidget::setShowGhostNodes(bool show) {
    if (m_showGhostNodes != show) {
        m_showGhostNodes = show;
        drawMap(cachedNodes, cachedEdges);
    }
}

// ---------------------------------------------------------
//  重绘逻辑
// ---------------------------------------------------------
// view/MapWidget.cpp

void MapWidget::drawMap(const QVector<Node>& nodes, const QVector<Edge>& edges)
{
    // 1. 停止所有动画和定时器，切断对 item 的引用
    if (animationTimer->isActive()) animationTimer->stop();
    stopHoverAnimations();
    
    // === 【关键修复开始】 ===
    // 必须清空这些辅助列表，否则它们会指向已经被 delete 的对象（野指针）
    // 导致下一次鼠标操作时直接崩溃！
    hoverItems.clear(); 
    dyingItems.clear();
    // === 【关键修复结束】 ===

    // 2. 清空所有的指针缓存
    nodeGraphicsItems.clear();
    nodeLabelItems.clear();
    edgeGraphicsItems.clear();
    nodeConnectedEdgeKeys.clear();
    
    activeTrackItem = nullptr;
    activeGrowthItem = nullptr;
    hoveredNodeId = -1; // 重置悬停状态
    hoveredEdgeIndex = -1;

    // 3. 安全清理场景 (保留背景和天气层)
    QList<QGraphicsItem*> allItems = scene->items();
    for (QGraphicsItem* item : allItems) {
        if (item != backgroundItem && item != weatherOverlay) {
            scene->removeItem(item);
            delete item;
        }
    }
    
    // 更新缓存数据
    cachedNodes = nodes;
    cachedEdges = edges;

    // 4. 重绘边 (Edge)
    // 先画边，这样边会在节点下面
    for(long long i=0; i<edges.size(); ++i) {
        const Edge& e = edges[i];
        
        // 查找端点坐标
        QPointF uPos, vPos;
        bool uFound=false, vFound=false;
        for(const auto& n : nodes) {
            if(n.id == e.u) { uPos = QPointF(n.x, n.y); uFound=true; }
            if(n.id == e.v) { vPos = QPointF(n.x, n.y); vFound=true; }
            if(uFound && vFound) break;
        }
        if(!uFound || !vFound) continue;

        QGraphicsLineItem* lineItem = scene->addLine(QLineF(uPos, vPos), edgePenForType(e.type));
        lineItem->setZValue(e.type == EdgeType::Stairs ? 6 : 5); 
        
        // 记录映射
        long long key = makeEdgeKey(e.u, e.v);
        edgeGraphicsItems.insert(key, lineItem);
        
        nodeConnectedEdgeKeys[e.u].append(key);
        nodeConnectedEdgeKeys[e.v].append(key);
    }

    // 5. 重绘节点 (Node)
    QFont nameFont("Microsoft YaHei", 8);
    for(const auto& n : nodes) {
        // 如果不显示幽灵节点且当前是 Ghost，则跳过
        if (!m_showGhostNodes && n.type == NodeType::Ghost) continue;

        double r = (n.type == NodeType::Ghost) ? 8.0 : 12.0; 
        
        // 节点圆圈
        QGraphicsEllipseItem* el = scene->addEllipse(-r/2, -r/2, r, r);
        el->setPos(n.x, n.y);
        el->setZValue(10);
        
        if (n.type == NodeType::Ghost) {
            el->setPen(QPen(Qt::NoPen));
            el->setBrush(QColor(0, 0, 0, 40)); 
        } else {
            el->setPen(QPen(Qt::white, 2));
            el->setBrush(QColor("#636366")); 
        }
        
        nodeGraphicsItems.insert(n.id, el);

        // 节点文字 (仅 Visible 节点)
        if (n.type == NodeType::Visible) {
            QGraphicsTextItem* label = scene->addText(n.name, nameFont);
            QRectF bd = label->boundingRect();
            label->setPos(n.x - bd.width()/2.0, n.y + r/2.0 + 2.0);
            label->setDefaultTextColor(Qt::black);
            label->setZValue(12);
            nodeLabelItems.insert(n.id, label);
            
            // 简单防遮挡：如果文字重叠则隐藏（可选优化）
            // 这里为了性能暂不处理复杂的碰撞检测
        }
    }
}

// =========================================================
//  【修复】鼠标点击事件 (去掉了导致闪退的 drawMap)
// =========================================================
void MapWidget::mousePressEvent(QMouseEvent *event) 
{
    QPointF scenePos = mapToScene(event->pos());
    
    // 1. 中键平移 (保持不变)
    if (event->button() == Qt::MiddleButton) {
        isMiddlePanning = true; 
        lastPanPos = event->pos(); 
        setCursor(Qt::ClosedHandCursor); 
        event->accept(); 
        return;
    }

    // 2. 右键处理 (取消连线 或 撤销)
    if (event->button() == Qt::RightButton) {
        if (currentMode == EditMode::None) {
            int hitId = findNodeAt(scenePos);
            if (hitId != -1) {
                QString name; for(const auto&n:cachedNodes) if(n.id==hitId) name=n.name;
                fadeOutHoverItems(); emit nodeClicked(hitId, name, false); 
            }
        } else {
            // 编辑模式下的右键
            if (currentMode == EditMode::ConnectEdge && connectFirstNodeId != -1) {
                // === 【修复】取消选中时，不要重绘全图，只取消高亮 ===
                updateNodeHighlight(connectFirstNodeId, false);
                connectFirstNodeId = -1; 
                this->viewport()->update(); // 刷新一下去除虚线
            } else { 
                emit undoRequested(); 
            }
        }
        event->accept(); return;
    }
    
    // 3. 左键处理
    if (event->button() == Qt::LeftButton) {
        int hitId = findNodeAt(scenePos);
        
        // 判断是否允许拖拽
        bool canDrag = false;
        if (m_isEditable) {
            if (currentMode == EditMode::None || 
                currentMode == EditMode::AddBuilding || 
                currentMode == EditMode::AddGhost) {
                canDrag = true;
            }
        }

        if (canDrag && hitId != -1) {
            emit nodeEditClicked(hitId, (currentMode != EditMode::None)); 
            draggingNodeId = hitId; 
            // drawMap(cachedNodes, cachedEdges); // <--- 【删除】这里其实不用重绘，直接拖就行
            isNodeDragging = true; 
            lastScenePos = scenePos;
            setCursor(Qt::SizeAllCursor); 
            event->accept(); 
            return;
        }

        // --- 模式分支 ---

        // 模式 A: 浏览 (选起点)
        if (currentMode == EditMode::None) {
            if (hitId != -1) {
                fadeOutHoverItems(); 
                QString name; for(const auto&n:cachedNodes) if(n.id==hitId) name=n.name;
                emit nodeClicked(hitId, name, true); 
                if (m_isEditable) {
                    emit nodeEditClicked(hitId, false);
                }
            }
        } 
        // 模式 B: 连线 (ConnectEdge) - 【重点修复区域】
        else if (currentMode == EditMode::ConnectEdge) {
            if (hitId != -1) {
                if (connectFirstNodeId == -1) {
                    // 选中第一个点
                    connectFirstNodeId = hitId;
                    // drawMap(...) <--- 【删除】绝对不能在这里重绘
                    updateNodeHighlight(connectFirstNodeId, true); // 仅高亮
                } else if (hitId != connectFirstNodeId) {
                    // 连到第二个点
                    updateNodeHighlight(connectFirstNodeId, false); // 取消第一个点高亮
                    emit edgeConnectionRequested(connectFirstNodeId, hitId);
                    connectFirstNodeId = -1; 
                    // drawMap(...) <--- 【删除】不要重绘，信号发出后 EditorWindow 会处理数据并刷新
                    this->viewport()->update();
                }
            }
        } 
        // 模式 C: 新建 (AddBuilding / AddGhost)
        else if (currentMode == EditMode::AddBuilding || currentMode == EditMode::AddGhost) {
            if (hitId == -1) emit emptySpaceClicked(scenePos.x(), scenePos.y());
        }
        event->accept();
    }
}

// ---------------------------------------------------------
//  高性能拖拽实现 (防御性编程版)
// ---------------------------------------------------------
void MapWidget::mouseMoveEvent(QMouseEvent *event) 
{
    QPointF scenePos = mapToScene(event->pos());
    
    // 1. 中键平移逻辑
    if (isMiddlePanning) {
        QPoint delta = event->pos() - lastPanPos;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        lastPanPos = event->pos(); 
        event->accept(); 
        return;
    }
    
    // 2. 节点拖拽逻辑 (保持流畅性)
    if (isNodeDragging && draggingNodeId != -1) {
        double dx = scenePos.x() - lastScenePos.x();
        double dy = scenePos.y() - lastScenePos.y();
        
        // 更新数据模型
        double newX = 0, newY = 0;
        for(auto& n : cachedNodes) {
            if(n.id == draggingNodeId) {
                n.x += dx; n.y += dy;
                newX = n.x; newY = n.y;
                lastScenePos = scenePos; 
                break;
            }
        }
        
        // 更新图元位置 (不重绘，高性能)
        if (nodeGraphicsItems.contains(draggingNodeId)) {
            auto item = nodeGraphicsItems[draggingNodeId];
            if (item && item->scene() == scene) {
                QRectF rect = item->rect();
                item->setRect(newX - rect.width()/2.0, newY - rect.height()/2.0, rect.width(), rect.height());
            }
        }
        
        // 更新文字位置
        if (nodeLabelItems.contains(draggingNodeId)) {
            auto text = nodeLabelItems[draggingNodeId];
            if (text && text->scene() == scene) {
                double r = 12.0; 
                QRectF bd = text->boundingRect();
                text->setPos(newX - bd.width()/2.0, newY + r + 6);
            }
        }

        // 更新相连边位置
        if (nodeConnectedEdgeKeys.contains(draggingNodeId)) {
            const auto& keys = nodeConnectedEdgeKeys[draggingNodeId];
            for(long long key : keys) {
                if (edgeGraphicsItems.contains(key)) {
                    auto line = edgeGraphicsItems[key];
                    if (line && line->scene() == scene) { 
                        QLineF l = line->line();
                        QPointF p1 = l.p1();
                        QPointF p2 = l.p2();
                        
                        // 简单的距离判断，决定更新哪一端
                        if ((p1 - QPointF(newX - dx, newY - dy)).manhattanLength() < 20.0) { 
                            l.setP1(QPointF(newX, newY));
                        } else {
                            l.setP2(QPointF(newX, newY));
                        }
                        line->setLine(l);
                    }
                }
            }
        }
        event->accept(); 
        return; 
    }
    
    // 3. 【修改】移除了连线模式下烦人的自动高亮逻辑
    // 现在连线模式下鼠标移动没有任何视觉干扰，清清爽爽
    if (currentMode == EditMode::ConnectEdge) {
        event->accept(); 
        return;
    }
    
    // 4. 普通模式(None)下的悬停气泡逻辑 (保持原样，仅用于展示信息)
    if (currentMode == EditMode::None && event->buttons() == Qt::NoButton) {
        int hitNode = findNodeAt(scenePos);
        if (hitNode != -1) {
            if (hoveredNodeId != hitNode || hoveredEdgeIndex != -1) {
                stopHoverAnimations(); clearHoverItems(); 
                hoveredNodeId = hitNode; hoveredEdgeIndex = -1;
                for (const auto& n : cachedNodes) { if (n.id == hitNode) { showNodeHoverBubble(n); break; } }
            }
            event->accept(); return;
        }
        
        QPointF closest; int u=-1, v=-1;
        int edgeIdx = findEdgeAt(scenePos, closest, u, v);
        
        if (edgeIdx != -1) {
            QString newName = cachedEdges[edgeIdx].name;
            QString oldName = (hoveredEdgeIndex != -1 && hoveredEdgeIndex < cachedEdges.size()) ? cachedEdges[hoveredEdgeIndex].name : "";
            bool sameRoad = (!newName.isEmpty() && newName != "路" && newName == oldName);

            if (!sameRoad && (hoveredEdgeIndex != edgeIdx || hoveredNodeId != -1)) {
                stopHoverAnimations(); clearHoverItems();
                hoveredEdgeIndex = edgeIdx; hoveredNodeId = -1;
                showEdgeHoverBubble(cachedEdges[edgeIdx], closest);
            } else if (sameRoad) {
                hoveredEdgeIndex = edgeIdx;
            }
            event->accept(); return;
        }
        
        if (hoveredNodeId != -1 || hoveredEdgeIndex != -1) {
            hoveredNodeId = -1; hoveredEdgeIndex = -1; fadeOutHoverItems(); 
        }
    }
    QGraphicsView::mouseMoveEvent(event);
}

void MapWidget::mouseReleaseEvent(QMouseEvent *event) 
{
    // 1. 中键平移释放
    if (isMiddlePanning && event->button() == Qt::MiddleButton) {
        isMiddlePanning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    // 2. 【核心修复】拖拽结束逻辑
    if (isNodeDragging) {
        if (draggingNodeId != -1) {
            // A. 从缓存中找到当前节点移动后的最终位置
            // (mouseMoveEvent 里已经更新了 cachedNodes 的坐标，这里直接读)
            double finalX = 0, finalY = 0;
            bool found = false;
            for(const auto& n : cachedNodes) {
                if(n.id == draggingNodeId) {
                    finalX = n.x;
                    finalY = n.y;
                    found = true;
                    break;
                }
            }

            // B. 发送正确的保存信号
            // 告诉 EditorWindow：“把 ID 为 draggingNodeId 的点存到 (finalX, finalY)”
            if (found) {
                emit nodeMoved(draggingNodeId, finalX, finalY);
            }
        }

        // C. 重置状态
        isNodeDragging = false;
        draggingNodeId = -1;
        setCursor(Qt::ArrowCursor);
    }

    // 3. 拦截编辑模式下的 Release，防止闪退
    if (currentMode != EditMode::None) {
        event->accept();
        return; 
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void MapWidget::wheelEvent(QWheelEvent *event) {
    fadeOutHoverItems(); 
    double viewW = viewport()->width(); double viewH = viewport()->height();
    double sceneW = scene->width(); double sceneH = scene->height();
    if (sceneW <= 0 || sceneH <= 0) { QGraphicsView::wheelEvent(event); return; }
    double minScaleX = viewW / sceneW; double minScaleY = viewH / sceneH;
    double minScale = std::max(minScaleX, minScaleY);
    double currentTransformScale = transform().m11(); const double scaleFactor = 1.15;
    double newScale = currentTransformScale;
    if (event->angleDelta().y() > 0) this->scale(scaleFactor, scaleFactor);
    else {
        newScale /= scaleFactor;
        if (newScale < minScale) {
            double factor = minScale / currentTransformScale; if (factor < 1.0) this->scale(factor, factor);
        } else this->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
    }
    event->accept();
}

void MapWidget::highlightPath(const QVector<int>& pathNodeIds, double animationDuration) {
    if (currentPathNodeIds == pathNodeIds && animationTimer->isActive()) return;
    clearPathHighlight();
    if (pathNodeIds.size() < 2) return;
    currentPathNodeIds = pathNodeIds; animationDurationMs = animationDuration * 1000.0;
    animationProgress = 0.0; animationStartTime = QDateTime::currentMSecsSinceEpoch();

    QPainterPath fullPath;
    QMap<int, Node> nodeMap; for(const auto& n : cachedNodes) nodeMap.insert(n.id, n);
    Node start = nodeMap[pathNodeIds[0]]; fullPath.moveTo(start.x, start.y);
    for(int i=1; i<pathNodeIds.size(); ++i) { Node n = nodeMap[pathNodeIds[i]]; fullPath.lineTo(n.x, n.y); }
    
    QPen trackPen(QColor(0, 122, 255, 40)); trackPen.setWidth(8); 
    trackPen.setCapStyle(Qt::RoundCap); trackPen.setJoinStyle(Qt::RoundJoin);
    activeTrackItem = scene->addPath(fullPath, trackPen); activeTrackItem->setZValue(15);
    animationTimer->start(16);
}

void MapWidget::onAnimationTick() {
    if (currentPathNodeIds.isEmpty()) { animationTimer->stop(); return; }
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - animationStartTime;
    animationProgress = (double)elapsed / animationDurationMs;
    if (animationProgress >= 1.0) { animationProgress = 1.0; animationTimer->stop(); }
    drawPathGrowthAnimation();
}

void MapWidget::drawPathGrowthAnimation() {
    // 【三级防线】动画重绘前检查对象是否存在
    if (activeGrowthItem) { 
        if(activeGrowthItem->scene() == scene) scene->removeItem(activeGrowthItem); 
        delete activeGrowthItem; 
        activeGrowthItem = nullptr; 
    }
    
    QMap<int, Node> nodeMap; for(const auto& n : cachedNodes) nodeMap.insert(n.id, n);
    QPainterPath currPath; Node n0 = nodeMap[currentPathNodeIds[0]]; currPath.moveTo(n0.x, n0.y);
    double totalSegs = currentPathNodeIds.size() - 1; double currentPos = animationProgress * totalSegs;
    int segIndex = (int)currentPos; double segLocalProgress = currentPos - segIndex;
    QPointF tipPos(n0.x, n0.y);
    for (int i = 0; i < segIndex && i < totalSegs; ++i) {
        Node n = nodeMap[currentPathNodeIds[i+1]]; currPath.lineTo(n.x, n.y); tipPos = QPointF(n.x, n.y);
    }
    if (segIndex < totalSegs) {
        Node start = nodeMap[currentPathNodeIds[segIndex]]; Node end = nodeMap[currentPathNodeIds[segIndex+1]];
        double dx = end.x - start.x; double dy = end.y - start.y;
        tipPos = QPointF(start.x + dx*segLocalProgress, start.y + dy*segLocalProgress); currPath.lineTo(tipPos);
    }
    QPen growPen(QColor("#007AFF")); growPen.setWidth(5); 
    growPen.setCapStyle(Qt::RoundCap); growPen.setJoinStyle(Qt::RoundJoin);
    activeGrowthItem = scene->addPath(currPath, growPen); activeGrowthItem->setZValue(20);
}

void MapWidget::clearPathHighlight() {
    if (animationTimer->isActive()) animationTimer->stop();
    animationProgress = 0.0; currentPathNodeIds.clear();
    
    if (activeTrackItem) { 
        if(activeTrackItem->scene() == scene) scene->removeItem(activeTrackItem); 
        delete activeTrackItem; activeTrackItem = nullptr; 
    }
    if (activeGrowthItem) { 
        if(activeGrowthItem->scene() == scene) scene->removeItem(activeGrowthItem); 
        delete activeGrowthItem; activeGrowthItem = nullptr; 
    }
    this->viewport()->update();
}

void MapWidget::pauseHoverAnimations() {
    for (auto &a : hoverAnims) if (a && a->state() == QAbstractAnimation::Running) a->pause();
}
void MapWidget::resumeHoverAnimations() {
    for (auto &a : hoverAnims) if (a && a->state() == QAbstractAnimation::Paused) a->resume();
}
void MapWidget::resizeEvent(QResizeEvent *event) {
    QGraphicsView::resizeEvent(event); 
    pauseHoverAnimations(); 
    if (hoverResumeTimer) hoverResumeTimer->start(300);
    
    if (weatherOverlay) {
        QPointF sceneTopLeft = mapToScene(0, 0);
        weatherOverlay->setPos(sceneTopLeft);
        weatherOverlay->setOverlayRect(QRectF(0, 0, event->size().width(), event->size().height()));
    }
}
void MapWidget::leaveEvent(QEvent *event) { Q_UNUSED(event); fadeOutHoverItems(); }

int MapWidget::findNodeAt(const QPointF& pos) {
    double threshold = 30.0; 
    for(const auto& node : cachedNodes) {
        bool isGhost = (node.type == NodeType::Ghost);
        bool forceShow = (currentMode == EditMode::ConnectEdge || 
                          currentMode == EditMode::AddBuilding || 
                          currentMode == EditMode::AddGhost);
                          
        if (isGhost && !m_showGhostNodes && !forceShow) continue;

        double dx = pos.x() - node.x; 
        double dy = pos.y() - node.y;
        if (dx*dx + dy*dy < threshold*threshold) return node.id;
    }
    return -1;
}

int MapWidget::findEdgeAt(const QPointF& pos, QPointF& closestPoint, int& outU, int& outV) {
    if (cachedEdges.isEmpty() || cachedNodes.isEmpty()) return -1;
    QMap<int, Node> nodeMap; for (const auto& n : cachedNodes) nodeMap.insert(n.id, n);
    int bestIdx = -1; double bestDist = 1e18; QPointF bestPt; int bu=-1, bv=-1; 
    const double threshold = 20.0; 
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
    QColor c(160, 160, 165); int width = 3;
    switch (type) {
    case EdgeType::Normal: c = QColor(160, 160, 165); width = 3; break;
    case EdgeType::Main:   c = QColor(160, 190, 220); width = 4; break;
    case EdgeType::Path:   c = QColor(160, 200, 160); width = 2; break;
    case EdgeType::Indoor: c = QColor(210, 180, 160); width = 2; break;
    case EdgeType::Stairs: c = QColor(255, 149, 0); width = 2; break; 
    }
    QPen pen(c); pen.setWidth(width); pen.setJoinStyle(Qt::RoundJoin); pen.setCapStyle(Qt::RoundCap);
    return pen;
}

void MapWidget::addEditVisualNode(int id, const QString& name, const QPointF& pos, int typeInt) {
}
void MapWidget::clearEditTempItems() {
}

void MapWidget::setWeather(Weather weatherType) {
    m_currentWeatherState = weatherType;
    if (weatherOverlay) {
        OverlayType ot = OverlayType::Sunny;
        if (weatherType == Weather::Rainy) ot = OverlayType::Rainy;
        if (weatherType == Weather::Snowy) ot = OverlayType::Snowy;
        weatherOverlay->setWeatherType(ot);
    }
}

void MapWidget::setBackgroundImage(const QString& path)
{
    m_bgPath = path; 
    QPixmap px(path);
    if (px.isNull()) return;
    
    if (backgroundItem) { 
        scene->removeItem(backgroundItem); 
        delete backgroundItem; 
        backgroundItem = nullptr;
    }
    
    scene->setSceneRect(0, 0, px.width(), px.height());
    backgroundItem = scene->addPixmap(px);
    backgroundItem->setZValue(-100);
    backgroundItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
}

void MapWidget::clearHoverItems() {
    for (int nid : hiddenLabelNodeIds) if (nodeLabelItems.contains(nid) && nodeLabelItems[nid]) {
        if(nodeLabelItems[nid] && nodeLabelItems[nid]->scene() == scene) nodeLabelItems[nid]->setVisible(true);
    }
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
    for (int nid : hiddenLabelNodeIds) if (nodeLabelItems.contains(nid) && nodeLabelItems[nid]) {
        if(nodeLabelItems[nid] && nodeLabelItems[nid]->scene() == scene) nodeLabelItems[nid]->setVisible(true);
    }
    hiddenLabelNodeIds.clear();
    stopHoverAnimations(); 
    dyingItems.append(hoverItems);
    hoverItems.clear();

    auto* group = new QParallelAnimationGroup(this);
    for (auto* it : dyingItems) {
        if (!it) continue;
        QGraphicsObject* obj = dynamic_cast<QGraphicsObject*>(it);
        if (obj) {
            auto* a_op = new QPropertyAnimation(obj, "opacity", this);
            a_op->setDuration(200);
            a_op->setStartValue(obj->opacity());
            a_op->setEndValue(0.0);
            group->addAnimation(a_op);
            HoverBubble* hb = qobject_cast<HoverBubble*>(obj);
            if (hb) {
                auto* a_sc = new QPropertyAnimation(hb, "bubbleScale", this);
                a_sc->setDuration(200);
                a_sc->setStartValue(hb->bubbleScale());
                a_sc->setEndValue(0.95); 
                group->addAnimation(a_sc);
            }
        } else {
            scene->removeItem(it); delete it;
        }
    }
    connect(group, &QAbstractAnimation::finished, this, [this, group]() { killDyingItems(); group->deleteLater(); });
    hoverAnims.push_back(QPointer<QAbstractAnimation>(group));
    group->start();
}

QColor MapWidget::withAlpha(const QColor& c, int alpha) { QColor t=c; t.setAlpha(alpha); return t; }

void MapWidget::startHoverAppearAnimation() {
    auto* group = new QParallelAnimationGroup(this);
    for (auto* it : hoverItems) {
        if (!it) continue;
        it->setOpacity(0.0);
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
    stopHoverAnimations(); killDyingItems(); clearHoverItems(); 
    auto* halo = new HaloItem(QPointF(node.x, node.y), 45.0); 
    scene->addItem(halo); hoverItems.push_back(halo);

    const QColor bubbleColor(255, 255, 255, 215); 
    HoverBubble* hb = new HoverBubble();
    hb->setIsEdge(false); hb->setBaseColor(bubbleColor);
    hb->setContent(node.name, node.description);
    hb->setCenterAt(QPointF(node.x, node.y));
    hb->setZValue(100);
    double screenW = this->viewport()->width();
    currentBubbleScale = (screenW / 1280.0) * 1.8; 
    currentBubbleScale = std::clamp(currentBubbleScale, 1.2, 3.0); 
    hb->setBubbleScale(currentBubbleScale);
    scene->addItem(hb); hoverItems.push_back(hb);
    
    if (nodeLabelItems.contains(node.id) && nodeLabelItems[node.id]) {
        if (nodeLabelItems[node.id]->scene() == scene) nodeLabelItems[node.id]->setVisible(false);
        hiddenLabelNodeIds.push_back(node.id);
    }
    startHoverAppearAnimation();
}

void MapWidget::showEdgeHoverBubble(const Edge& edge, const QPointF& closestPoint) {
    if (edge.name.isEmpty() || edge.name == "路") return;

    stopHoverAnimations(); killDyingItems(); clearHoverItems();
    
    QMap<int, Node> nodeMap; 
    for (const auto& n : cachedNodes) nodeMap.insert(n.id, n);

    QVector<const Edge*> sameNameEdges;
    for (const auto& e : cachedEdges) {
        if (e.name == edge.name) {
            sameNameEdges.append(&e);
        }
    }

    for (const Edge* e : sameNameEdges) {
        if (!nodeMap.contains(e->u) || !nodeMap.contains(e->v)) continue;
        Node u = nodeMap[e->u]; 
        Node v = nodeMap[e->v];
        auto* glow = new GlowItem(QPointF(u.x, u.y), QPointF(v.x, v.y), 24.0); 
        scene->addItem(glow); 
        hoverItems.push_back(glow);
    }

    if (!nodeMap.contains(edge.u) || !nodeMap.contains(edge.v)) return;
    Node u = nodeMap[edge.u]; Node v = nodeMap[edge.v];

    const QColor baseEdgeColor = edgePenForType(edge.type).color();
    QColor bubbleColor = baseEdgeColor.lighter(170); bubbleColor.setAlpha(225); 
    
    HoverBubble* hb = new HoverBubble();
    hb->setIsEdge(true); hb->setBaseColor(bubbleColor);
    hb->setContent(edge.name, edge.description);
    
    QPointF A(u.x, u.y), B(v.x, v.y);
    hb->setEdgeLine(A, B);
    
    hb->setZValue(100);
    double screenW = this->viewport()->width();
    currentBubbleScale = (screenW / 1280.0) * 1.8;
    currentBubbleScale = std::clamp(currentBubbleScale, 1.2, 3.0);
    hb->setBubbleScale(currentBubbleScale);
    scene->addItem(hb); hoverItems.push_back(hb);
    
    startHoverAppearAnimation();
}

void MapWidget::setEditMode(EditMode mode) {
    currentMode = mode;
    connectFirstNodeId = -1; setActiveEdge(-1, -1); 
    clearPathHighlight(); fadeOutHoverItems(); 
    drawMap(cachedNodes, cachedEdges); 
}

// =========================================================
//  【新增】轻量级高亮函数，防止重绘闪退
// =========================================================
void MapWidget::updateNodeHighlight(int nodeId, bool highlight) 
{
    if (nodeId == -1) return;
    if (!nodeGraphicsItems.contains(nodeId)) return;

    QGraphicsItem* item = nodeGraphicsItems[nodeId];
    if (!item) return; // 防御性编程

    QGraphicsEllipseItem* ellipse = dynamic_cast<QGraphicsEllipseItem*>(item);
    if (ellipse) {
        if (highlight) {
            // 高亮样式：蓝色填充，白色加粗边框
            QPen p = ellipse->pen();
            p.setColor(Qt::white);
            p.setWidth(4);
            ellipse->setPen(p);
            ellipse->setBrush(QColor("#007AFF")); 
            ellipse->setZValue(100); // 让高亮的点浮在最上面
        } else {
            // 恢复普通样式 (灰色)
            QPen p(Qt::white);
            p.setWidth(3);
            ellipse->setPen(p);
            ellipse->setBrush(QColor("#636366")); 
            ellipse->setZValue(10);
        }
    }
}