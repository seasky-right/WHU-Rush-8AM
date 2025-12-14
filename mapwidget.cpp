#include "MapWidget.h"
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QMouseEvent>
#include <QDebug>
#include <cmath>

MapWidget::MapWidget(QWidget *parent) : QGraphicsView(parent)
{
    scene = new QGraphicsScene(this);
    this->setScene(scene);
    this->setRenderHint(QPainter::Antialiasing);
    this->setBackgroundBrush(QBrush(QColor(240, 240, 245)));

    // 允许鼠标追踪 (为了后续做悬停效果，暂时先加上)
    this->setMouseTracking(true);
}

void MapWidget::drawMap(const QVector<Node>& nodes, const QVector<Edge>& edges)
{
    scene->clear();
    cachedNodes = nodes; // 保存数据供查找使用

    // 1. 画边
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
            line->setZValue(0); // 层级：最底层
        }
    }

    // 2. 画节点
    QBrush nodeBrush(QColor("white"));
    QPen nodePen(QColor("#2C3E50"));
    nodePen.setWidth(2);

    for(const auto& node : nodes) {
        double r = 8.0;
        auto item = scene->addEllipse(node.x - r, node.y - r, 2*r, 2*r, nodePen, nodeBrush);
        item->setZValue(2); // 层级：最顶层
        item->setData(0, node.id); // 把 ID 藏在 item 数据里

        // 文字标签
        if (!node.name.isEmpty()) {
            auto text = scene->addText(node.name);
            text->setPos(node.x - 10, node.y + 5);
            text->setDefaultTextColor(QColor("#555"));
            text->setZValue(2);
        }
    }

    if (!nodes.isEmpty()) {
        this->fitInView(scene->itemsBoundingRect(), Qt::KeepAspectRatio);
    }
}

void MapWidget::mousePressEvent(QMouseEvent *event)
{
    // 将 View 坐标转换为 Scene 坐标
    QPointF scenePos = mapToScene(event->pos());
    // qDebug() << "Mouse Press at Scene Pos:" << scenePos; // 调试输出点击坐标

    // 查找点击位置附近的节点
    int nodeId = findNodeAt(scenePos);

    if (nodeId != -1) {
        // 找到了！触发信号
        QString name = "";
        for(auto n : cachedNodes) if(n.id == nodeId) name = n.name;

        bool isLeft = (event->button() == Qt::LeftButton);
        emit nodeClicked(nodeId, name, isLeft);

        qDebug() << "选中节点:" << name << (isLeft ? "[起点]" : "[终点]");
        return; // 既然是点选，就不传递给父类了，防止变成拖拽
    }

    // 必须调用基类处理，否则拖拽可能失效
    QGraphicsView::mousePressEvent(event);
}

// 增加滚轮缩放功能
void MapWidget::wheelEvent(QWheelEvent *event)
{
    // 滚轮缩放
    double scaleFactor = 1.15;
    if(event->angleDelta().y() > 0) {
        this->scale(scaleFactor, scaleFactor);
    } else {
        this->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
    }
}

int MapWidget::findNodeAt(const QPointF& pos)
{
    double threshold = 25.0; // 点击灵敏度 (像素)

    for(const auto& node : cachedNodes) {
        double dx = pos.x() - node.x;
        double dy = pos.y() - node.y;
        double dist = std::sqrt(dx*dx + dy*dy);

        if (dist < threshold) {
            return node.id;
        }
    }
    return -1; // 没点中
}

void MapWidget::setStartNode(int nodeId) {
    // TODO: 实现变色逻辑 (下一阶段做)
}

void MapWidget::setEndNode(int nodeId) {
    // TODO: 实现变色逻辑
}
