#include "MapWidget.h"
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QMouseEvent>
#include <QDebug>
#include <cmath>
#include <QGraphicsPixmapItem>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>
#include <QDateTime>

MapWidget::MapWidget(QWidget *parent) : QGraphicsView(parent)
{
    scene = new QGraphicsScene(this);
    this->setScene(scene);
    this->setRenderHint(QPainter::Antialiasing);
    this->setBackgroundBrush(QBrush(QColor(240, 240, 245)));
    this->setMouseTracking(true);
    
    // 初始化动画计时器
    animationTimer = new QTimer(this);
    connect(animationTimer, &QTimer::timeout, this, &MapWidget::onAnimationTick);
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

    QPen edgePen(QColor(200, 200, 200));
    edgePen.setWidth(3);
    edgePen.setCapStyle(Qt::RoundCap);

    QMap<int, Node> nodeMap;
    for(const auto& node : nodes) nodeMap.insert(node.id, node);

    for(const auto& edge : edges) {
        if(nodeMap.contains(edge.u) && nodeMap.contains(edge.v)) {
            Node start = nodeMap[edge.u];
            Node end = nodeMap[edge.v];
            auto line = scene->addLine(start.x, start.y, end.x, end.y, edgePen);
            line->setZValue(10);
        }
    }

    QBrush nodeBrush(QColor("white"));
    QPen nodePen(QColor("#2C3E50"));
    nodePen.setWidth(2);

    for(const auto& node : nodes) {
        double r = 8.0;
        auto item = scene->addEllipse(node.x - r, node.y - r, 2*r, 2*r, nodePen, nodeBrush);
        item->setZValue(10);
        item->setData(0, node.id);

        if (!node.name.isEmpty()) {
            auto text = scene->addText(node.name);
            text->setPos(node.x - 10, node.y + 5);
            text->setDefaultTextColor(QColor("#555"));
            text->setZValue(10);
        }
    }

    if (!nodes.isEmpty()) {
        this->fitInView(scene->itemsBoundingRect(), Qt::KeepAspectRatio);
        // 重置缩放因子为 1.0（因为 fitInView 改变了 view 的 transform）
        currentScale = 1.0;
    }
}

void MapWidget::mousePressEvent(QMouseEvent *event)
{
    QPointF scenePos = mapToScene(event->pos());
    int nodeId = findNodeAt(scenePos);

    if (nodeId != -1) {
        QString name = "";
        for(auto n : cachedNodes) if(n.id == nodeId) name = n.name;

        bool isLeft = (event->button() == Qt::LeftButton);
        emit nodeClicked(nodeId, name, isLeft);

        qDebug() << "选中节点:" << name << (isLeft ? "[起点]" : "[终点]");
        return;
    }

    // 左键未点中节点：进入拖动模式
    if (event->button() == Qt::LeftButton) {
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

    QGraphicsView::mouseMoveEvent(event);
}

void MapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    // 停止拖动模式
    if (event->button() == Qt::LeftButton && isDragging) {
        isDragging = false;
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
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

    // 如果 scene 中有节点，按节点范围缩放背景
    QRectF sceneBounds = scene->itemsBoundingRect();
    if (!sceneBounds.isEmpty()) {
        // 缩放背景 pixmap 以匹配 scene 中节点的范围
        QSize targetSize(qMax(1, int(sceneBounds.width())), qMax(1, int(sceneBounds.height())));
        QPixmap scaled = px.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        
        // 创建背景项并设置位置
        backgroundItem = scene->addPixmap(scaled);
        backgroundItem->setPos(sceneBounds.topLeft());
    } else {
        // 如果 scene 为空，暂时创建空的背景项
        backgroundItem = scene->addPixmap(QPixmap());
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

void MapWidget::setStartNode(int nodeId) {
}

void MapWidget::setEndNode(int nodeId) {
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
    
    // 首先绘制静态的路径背景（浅蓝色，用于高亮）
    QMap<int, Node> nodeMap;
    for(const auto& node : cachedNodes) {
        nodeMap.insert(node.id, node);
    }
    
    // 绘制完整路径的高亮背景
    for (int i = 0; i < pathNodeIds.size() - 1; ++i) {
        int u = pathNodeIds[i];
        int v = pathNodeIds[i + 1];
        
        if (nodeMap.contains(u) && nodeMap.contains(v)) {
            Node startNode = nodeMap[u];
            Node endNode = nodeMap[v];
            
            // 绘制浅蓝色高亮线（宽度较粗）
            QPen highlightPen(QColor(100, 180, 240, 200));  // 较浅的蓝色，带透明度
            highlightPen.setWidth(6);
            highlightPen.setCapStyle(Qt::RoundCap);
            highlightPen.setJoinStyle(Qt::RoundJoin);
            
            auto highlightLine = scene->addLine(startNode.x, startNode.y, endNode.x, endNode.y, highlightPen);
            highlightLine->setZValue(15);  // 在原路径上方
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
    // 首先删除之前的生长线（ZValue=20的所有项）
    auto items = scene->items();
    for (auto item : items) {
        if (item->zValue() == 20) {
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
    
    // 绘制从起点到当前位置的生长路径
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
        
        // 使用更鲜艳的蓝色绘制生长线
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

void MapWidget::clearPathHighlight()
{
    animationTimer->stop();
    animationProgress = 0.0;
    currentPathNodeIds.clear();
    
    if (pathGrowthLine) {
        scene->removeItem(pathGrowthLine);
        delete pathGrowthLine;
        pathGrowthLine = nullptr;
    }
    
    // 移除所有高亮线（Z值为15或20的项）
    auto items = scene->items();
    for (auto item : items) {
        if (item->zValue() >= 15 && item->zValue() <= 20) {
            scene->removeItem(item);
            delete item;
        }
    }
    
    this->viewport()->update();
}

