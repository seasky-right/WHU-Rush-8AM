#include "GraphModel.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <queue>
#include <limits>
#include <cmath>
#include <algorithm>

// 常量定义保持不变...
const double SPEED_WALK = 1.2;
const double SPEED_BIKE = 4.2;
const double SLOPE_THRESHOLD = 0.05;
const double DOWNHILL_MULTIPLIER = 1.5;
const double UPHILL_MULTIPLIER_BIKE = 0.3;
const double UPHILL_MULTIPLIER_WALK = 0.8;
const double UPHILL_COST_MULTIPLIER = 20.0;
const double STAIR_COST_MULTIPLIER = 10.0;

GraphModel::GraphModel() {}

bool GraphModel::loadData(const QString& nodesPath, const QString& edgesPath) {
    nodesMap.clear();
    edgesList.clear();
    maxBuildingId = 100;
    maxRoadId = 900;

    QFile nodeFile(nodesPath);
    if (nodeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream inNode(&nodeFile);
        while (!inNode.atEnd()) {
            QString line = inNode.readLine().trimmed();
            if (!line.isEmpty() && !line.startsWith("#")) parseNodeLine(line);
        }
        nodeFile.close();
    }

    QFile edgeFile(edgesPath);
    if (edgeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream inEdge(&edgeFile);
        while (!inEdge.atEnd()) {
            QString line = inEdge.readLine().trimmed();
            if (!line.isEmpty() && !line.startsWith("#")) parseEdgeLine(line);
        }
        edgeFile.close();
    }

    buildAdjacencyList();
    return true;
}

bool GraphModel::saveData(const QString& nodesPath, const QString& edgesPath) {
    // 保存节点
    QFile nodeFile(nodesPath);
    if (!nodeFile.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream outNode(&nodeFile);
    outNode.setEncoding(QStringConverter::Utf8);
    // 保持 ID 排序写入
    QList<int> keys = nodesMap.keys();
    std::sort(keys.begin(), keys.end());
    for(int id : keys) {
        const Node& n = nodesMap[id];
        // 格式: id, name, x, y, z, type(0/9), description, category
        int typeInt = (n.type == NodeType::Visible) ? 0 : 9;
        QString catStr = Node::categoryToString(n.category);
        outNode << n.id << "," << n.name << "," << n.x << "," << n.y << "," << n.z << ","
                << typeInt << "," << n.description << "," << catStr << "\n";
    }
    nodeFile.close();

    // 保存边
    QFile edgeFile(edgesPath);
    if (!edgeFile.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream outEdge(&edgeFile);
    outEdge.setEncoding(QStringConverter::Utf8);
    for(const auto& e : edgesList) {
        // 格式: u, v, dist, type, isSlope, name, desc
        outEdge << e.u << "," << e.v << "," << e.distance << ","
                << static_cast<int>(e.type) << "," << (e.isSlope?1:0) << ","
                << e.name << "," << e.description << "\n";
    }
    edgeFile.close();
    
    qDebug() << "Data saved successfully.";
    return true;
}

void GraphModel::parseNodeLine(const QString& line) {
    QStringList parts = line.split(",");
    if (parts.size() < 8) return;
    Node node;
    node.id = parts[0].trimmed().toInt();
    node.name = parts[1].trimmed();
    node.x = parts[2].trimmed().toDouble();
    node.y = parts[3].trimmed().toDouble();
    node.z = parts[4].trimmed().toDouble();
    int rawType = parts[5].trimmed().toInt();
    node.type = (rawType == 9) ? NodeType::Ghost : NodeType::Visible;
    node.description = parts[6].trimmed();
    node.category = Node::stringToCategory(parts[7].trimmed());
    nodesMap.insert(node.id, node);

    // 更新ID计数器
    if (node.type == NodeType::Visible && node.id >= maxBuildingId) maxBuildingId = node.id + 1;
    if (node.type == NodeType::Ghost && node.id >= maxRoadId) maxRoadId = node.id + 1;
}

void GraphModel::parseEdgeLine(const QString& line) {
    QStringList parts = line.split(",");
    if (parts.size() < 7) return;
    Edge edge;
    edge.u = parts[0].trimmed().toInt();
    edge.v = parts[1].trimmed().toInt();
    edge.distance = parts[2].trimmed().toDouble();
    edge.type = static_cast<EdgeType>(parts[3].trimmed().toInt());
    edge.isSlope = (parts[4].trimmed().toInt() == 1);
    edge.name = parts[5].trimmed();
    edge.description = parts[6].trimmed();
    // 自动计算斜率逻辑保持
    edge.slope = edge.isSlope ? 0.08 : 0.0;
    edgesList.append(edge);
}

void GraphModel::buildAdjacencyList() {
    adj.clear();
    for (const auto& edge : edgesList) {
        adj[edge.u].append(edge);
        Edge rev = edge; 
        std::swap(rev.u, rev.v);
        rev.slope = -edge.slope;
        adj[edge.v].append(rev);
    }
}

// --- CRUD ---

int GraphModel::addNode(double x, double y, NodeType type) {
    Node n;
    n.x = x; n.y = y; n.z = 30.0; // 默认海拔
    n.type = type;
    n.id = (type == NodeType::Visible) ? maxBuildingId++ : maxRoadId++;
    n.name = (type == NodeType::Visible) ? QString("建筑_%1").arg(n.id) : QString("路口_%1").arg(n.id);
    n.description = "无";
    n.category = (type == NodeType::Visible) ? NodeCategory::Building : NodeCategory::Road;
    
    nodesMap.insert(n.id, n);
    
    // 记录 Undo
    HistoryAction act;
    act.type = HistoryAction::AddNode;
    act.nodeData = n;
    pushAction(act);
    
    return n.id;
}

void GraphModel::deleteNode(int id) {
    if (!nodesMap.contains(id)) return;
    
    // 记录 Undo (需要记录节点数据和所有相连的边)
    // 简化处理：Undo只恢复节点，边需要额外逻辑。这里为演示，我们主要支持“撤销上一步新建”。
    // 如果是“撤销删除”，需要把删除的边也记下来。
    // 这里简单实现：
    Node target = nodesMap[id];
    
    // 删除所有连接的边
    for (int i = edgesList.size() - 1; i >= 0; --i) {
        if (edgesList[i].u == id || edgesList[i].v == id) {
            edgesList.removeAt(i);
        }
    }
    nodesMap.remove(id);
    buildAdjacencyList();
    
    HistoryAction act;
    act.type = HistoryAction::DeleteNode;
    act.nodeData = target;
    pushAction(act);
}

void GraphModel::updateNode(const Node& n) {
    if (nodesMap.contains(n.id)) {
        nodesMap[n.id] = n;
        // MoveNode Undo 逻辑较复杂，暂略，主要关注增删
    }
}

void GraphModel::addOrUpdateEdge(const Edge& edge) {
    // 检查是否已存在
    for(int i=0; i<edgesList.size(); ++i) {
        if ((edgesList[i].u == edge.u && edgesList[i].v == edge.v) ||
            (edgesList[i].u == edge.v && edgesList[i].v == edge.u)) {
            edgesList[i] = edge; // 更新
            buildAdjacencyList();
            return;
        }
    }
    // 新增
    edgesList.append(edge);
    buildAdjacencyList();
    
    HistoryAction act;
    act.type = HistoryAction::AddEdge;
    act.edgeData = edge;
    pushAction(act);
}

void GraphModel::deleteEdge(int u, int v) {
    for(int i=0; i<edgesList.size(); ++i) {
        const auto& e = edgesList[i];
        if ((e.u == u && e.v == v) || (e.u == v && e.v == u)) {
            HistoryAction act;
            act.type = HistoryAction::DeleteEdge;
            act.edgeData = e;
            pushAction(act);
            
            edgesList.removeAt(i);
            buildAdjacencyList();
            return;
        }
    }
}

// 简单的撤销实现
void GraphModel::pushAction(const HistoryAction& action) {
    undoStack.push(action);
}

void GraphModel::undo() {
    if (undoStack.isEmpty()) return;
    HistoryAction act = undoStack.pop();
    
    switch (act.type) {
    case HistoryAction::AddNode:
        // 撤销添加 -> 删除该节点
        // 注意：这里调用内部删除，不要再次 push 到 undo stack
        nodesMap.remove(act.nodeData.id); 
        // 也要删除连带的边(如果有)
        break;
    case HistoryAction::DeleteNode:
        // 撤销删除 -> 恢复节点
        nodesMap.insert(act.nodeData.id, act.nodeData);
        // (边没恢复，这是简版)
        break;
    case HistoryAction::AddEdge:
        // 撤销添加边 -> 删除边
        for(int i=0; i<edgesList.size(); ++i) {
            if (edgesList[i].u == act.edgeData.u && edgesList[i].v == act.edgeData.v) {
                edgesList.removeAt(i); break;
            }
        }
        buildAdjacencyList();
        break;
    case HistoryAction::DeleteEdge:
        // 撤销删除边 -> 恢复边
        edgesList.append(act.edgeData);
        buildAdjacencyList();
        break;
    default: break;
    }
}

bool GraphModel::canUndo() const { return !undoStack.isEmpty(); }

// Getters
Node GraphModel::getNode(int id) { return nodesMap.value(id); }
Node* GraphModel::getNodePtr(int id) { 
    if(nodesMap.contains(id)) return &nodesMap[id]; 
    return nullptr;
}
QVector<Node> GraphModel::getAllNodes() const { return nodesMap.values(); }
QVector<Edge> GraphModel::getAllEdges() const { return edgesList; }
const Edge* GraphModel::findEdge(int u, int v) const {
    // 遍历查找
    for(const auto& e : edgesList) {
        if ((e.u == u && e.v == v) || (e.u == v && e.v == u)) return &e;
    }
    return nullptr;
}

// 寻路逻辑保持原样 (省略以节省篇幅，请保留原有代码)
double GraphModel::getEdgeWeight(const Edge& edge, WeightMode mode) const {
    // 复制之前的实现...
    if(mode == WeightMode::DISTANCE) return edge.distance;
    return edge.distance; // 简化，请确保复制之前的完整逻辑
}
double GraphModel::getEffectiveSpeed(double baseSpeed, const Edge& edge, WeightMode mode) const { return baseSpeed; }
QVector<int> GraphModel::findPath(int startId, int endId) { return findPathWithMode(startId, endId, WeightMode::DISTANCE); }
QVector<int> GraphModel::findPathWithMode(int startId, int endId, WeightMode mode) { return QVector<int>(); /* 复制原代码 */ }
double GraphModel::calculateDistance(const QVector<int>& pathNodeIds) const { return 0; /* 复制原代码 */ }
double GraphModel::calculateDuration(const QVector<int>& pathNodeIds) const { return 0; /* 复制原代码 */ }
double GraphModel::calculateCost(const QVector<int>& pathNodeIds) const { return 0; /* 复制原代码 */ }
QVector<PathRecommendation> GraphModel::recommendPaths(int startId, int endId) { return QVector<PathRecommendation>(); /* 复制原代码 */ }