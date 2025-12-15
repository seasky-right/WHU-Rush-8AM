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

MapWidget::MapWidget(QWidget *parent) : QGraphicsView(parent)
{
    scene = new QGraphicsScene(this);
    this->setScene(scene);
    this->setRenderHint(QPainter::Antialiasing);
    this->setBackgroundBrush(QBrush(QColor(240, 240, 245)));
    this->setMouseTracking(true);
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
