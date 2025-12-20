#include "GraphModel.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <queue>
#include <limits>
#include <cmath>
#include <algorithm>
#include <QStringConverter>

// 常量定义
const double SPEED_WALK = 1.2;              // m/s
const double SLOPE_THRESHOLD = 0.05;        // 5%

GraphModel::GraphModel() {
    maxBuildingId = 100;
    maxRoadId = 10000;
}

// ------------------- 加载与保存 -------------------

bool GraphModel::loadData(const QString& nodesPath, const QString& edgesPath) {
    nodesMap.clear();
    edgesList.clear();
    // 【注意】这里要根据你们的分工修改起始ID！
    // 如果你是同学A，用 1000 / 10000
    // 如果你是同学B，用 3000 / 20000 ...
    maxBuildingId = 100; 
    maxRoadId = 10000;

    // 1. 加载节点
    QFile nodeFile(nodesPath);
    if (nodeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&nodeFile);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty() && !line.startsWith("#")) {
                QStringList parts = line.split(",");
                if (parts.size() < 6) continue;
                
                Node node;
                node.id = parts[0].toInt();
                node.name = parts[1].trimmed();
                node.x = parts[2].toDouble();
                node.y = parts[3].toDouble();
                node.z = parts[4].toDouble();
                int typeInt = parts[5].toInt();
                node.type = (typeInt == 9) ? NodeType::Ghost : NodeType::Visible;
                
                if (parts.size() > 7) {
                    node.description = parts[6].trimmed();
                    node.category = Node::stringToCategory(parts[7].trimmed());
                } else {
                    node.description = "无";
                    node.category = NodeCategory::None;
                }
                nodesMap.insert(node.id, node);

                if (node.type == NodeType::Visible && node.id >= maxBuildingId) maxBuildingId = node.id + 1;
                if (node.type == NodeType::Ghost && node.id >= maxRoadId) maxRoadId = node.id + 1;
            }
        }
        nodeFile.close();
    } else {
        qDebug() << "❌ Error: 无法打开节点文件:" << nodesPath;
        // 如果文件不存在，返回 true 允许新建
        // return false; 
    }

    // 2. 加载边
    QFile edgeFile(edgesPath);
    if (edgeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&edgeFile);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty() && !line.startsWith("#")) {
                QStringList parts = line.split(",");
                if (parts.size() < 3) continue;

                Edge edge;
                edge.u = parts[0].toInt();
                edge.v = parts[1].toInt();
                edge.distance = parts[2].toDouble();
                
                if (parts.size() > 3) edge.type = static_cast<EdgeType>(parts[3].toInt());
                else edge.type = EdgeType::Normal;

                // 【核心修改】这里直接读取 double 类型的 slope
                // 不再读取 isSlope (int 0/1)
                if (parts.size() > 4) edge.slope = parts[4].toDouble();
                else edge.slope = 0.0;
                
                if (parts.size() > 5) edge.name = parts[5].trimmed();
                if (parts.size() > 6) edge.description = parts[6].trimmed();
                
                edgesList.append(edge);
            }
        }
        edgeFile.close();
    } else {
        qDebug() << "❌ Error: 无法打开边文件:" << edgesPath;
    }

    // 3. 构建邻接表
    buildAdjacencyList();
    qDebug() << "✅ 数据加载完毕: Nodes=" << nodesMap.size() << " Edges=" << edgesList.size();
    return true;
}

bool GraphModel::saveData(const QString& nodesPath, const QString& edgesPath) {
    // 保存节点
    QFile nodeFile(nodesPath);
    if (!nodeFile.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream outNode(&nodeFile);
    outNode.setEncoding(QStringConverter::Utf8);
    
    QList<int> keys = nodesMap.keys();
    std::sort(keys.begin(), keys.end());
    
    for(int id : keys) {
        const Node& n = nodesMap[id];
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
        // 【核心修改】第5列写入 double 类型的 slope
        outEdge << e.u << "," << e.v << "," << e.distance << ","
                << static_cast<int>(e.type) << "," << e.slope << ","
                << e.name << "," << e.description << "\n";
    }
    edgeFile.close();
    return true;
}

void GraphModel::buildAdjacencyList() {
    adj.clear();
    for (const auto& edge : edgesList) {
        adj[edge.u].append(edge);
        
        // 添加反向边 (无向图)
        Edge rev = edge;
        std::swap(rev.u, rev.v);
        
        // 【核心修改】直接对 slope 取反，不再依赖 isSlope
        rev.slope = -edge.slope; 
        
        adj[edge.v].append(rev);
    }
}

// ------------------- 编辑器 CRUD -------------------

int GraphModel::addNode(double x, double y, NodeType type) {
    int id = (type == NodeType::Visible) ? (maxBuildingId++) : (maxRoadId++);
    Node n;
    n.id = id;
    
    // 根据类型自动命名
    if (type == NodeType::Visible) {
        n.name = QString("建筑_%1").arg(id);
        n.category = NodeCategory::None; // 建筑默认为 None，等待用户指定
    } else {
        n.name = QString("路口_%1").arg(id);
        // 【核心修改】新建路口时，默认分类设为 Road
        n.category = NodeCategory::Road; 
    }

    n.x = x; n.y = y; n.z = 30.0;
    n.type = type;
    n.description = "无";
    
    nodesMap.insert(id, n);
    
    // 记录 Undo ... (保持原有代码不变)
    HistoryAction act;
    act.type = HistoryAction::AddNode;
    act.nodeData = n;
    undoStack.push(act);
    
    return id;
}

void GraphModel::deleteNode(int id) {
    if (!nodesMap.contains(id)) return;
    
    Node target = nodesMap[id];
    
    // 同时也需要删除所有相关的边
    QVector<Edge> relatedEdges;
    for (int i = edgesList.size() - 1; i >= 0; --i) {
        if (edgesList[i].u == id || edgesList[i].v == id) {
            relatedEdges.append(edgesList[i]);
            edgesList.removeAt(i);
        }
    }
    nodesMap.remove(id);
    buildAdjacencyList();
    
    // 简化的 Undo: 只记录节点删除，未记录边删除（为了代码简洁，生产环境需完善）
    HistoryAction act;
    act.type = HistoryAction::DeleteNode;
    act.nodeData = target;
    undoStack.push(act);
}

void GraphModel::updateNode(const Node& n) {
    if (nodesMap.contains(n.id)) {
        nodesMap[n.id] = n;
    }
}

void GraphModel::addOrUpdateEdge(const Edge& edge) {
    // 检查是否存在
    bool found = false;
    for(int i=0; i<edgesList.size(); ++i) {
        if ((edgesList[i].u == edge.u && edgesList[i].v == edge.v) ||
            (edgesList[i].u == edge.v && edgesList[i].v == edge.u)) {
            edgesList[i] = edge;
            found = true;
            break;
        }
    }
    if (!found) {
        edgesList.append(edge);
        HistoryAction act;
        act.type = HistoryAction::AddEdge;
        act.edgeData = edge;
        undoStack.push(act);
    }
    buildAdjacencyList();
}

void GraphModel::deleteEdge(int u, int v) {
    for(int i=0; i<edgesList.size(); ++i) {
        const auto& e = edgesList[i];
        if ((e.u == u && e.v == v) || (e.u == v && e.v == u)) {
            HistoryAction act;
            act.type = HistoryAction::DeleteEdge;
            act.edgeData = e;
            undoStack.push(act);
            
            edgesList.removeAt(i);
            buildAdjacencyList();
            return;
        }
    }
}

void GraphModel::undo() {
    if (undoStack.isEmpty()) return;
    HistoryAction act = undoStack.pop();
    
    switch (act.type) {
    case HistoryAction::AddNode:
        nodesMap.remove(act.nodeData.id); 
        // 还要删掉刚才可能连带建立的边，这里简化
        break;
    case HistoryAction::DeleteNode:
        nodesMap.insert(act.nodeData.id, act.nodeData);
        break;
    case HistoryAction::AddEdge:
        deleteEdge(act.edgeData.u, act.edgeData.v); // 这会再次push，需要注意。简化版忽略递归
        undoStack.pop(); // 弹出刚才 deleteEdge 产生的 undo
        break;
    case HistoryAction::DeleteEdge:
        edgesList.append(act.edgeData);
        buildAdjacencyList();
        break;
    case HistoryAction::MoveNode:
        nodesMap[act.nodeData.id] = act.nodeData; // 恢复旧坐标
        break;
    }
}

bool GraphModel::canUndo() const { return !undoStack.isEmpty(); }

// ------------------- 寻路逻辑 -------------------

double GraphModel::getEdgeWeight(const Edge& edge, WeightMode mode) const {
    if (mode == WeightMode::DISTANCE) return edge.distance;
    
    double speed = SPEED_WALK; 
    // 简单坡度逻辑
    if (std::abs(edge.slope) > SLOPE_THRESHOLD) {
        // 上坡减速
        if (edge.slope > 0) speed *= 0.8; 
        // 下坡加速
        else speed *= 1.2; 
    }

    if (mode == WeightMode::TIME) return edge.distance / speed;
    
    if (mode == WeightMode::COST) {
        // 代价模式：非常讨厌上坡
        double cost = edge.distance;
        if (edge.slope > SLOPE_THRESHOLD) cost *= 5.0; // 上坡代价极大
        return cost;
    }
    return edge.distance;
}

QVector<int> GraphModel::findPathWithMode(int startId, int endId, WeightMode mode) {
    QVector<int> path;
    if (!nodesMap.contains(startId) || !nodesMap.contains(endId)) return path;

    QMap<int, double> dist;
    QMap<int, int> parent;
    for (int id : nodesMap.keys()) dist[id] = std::numeric_limits<double>::max();
    dist[startId] = 0;

    std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<std::pair<double, int>>> pq;
    pq.push({0, startId});

    while (!pq.empty()) {
        double d = pq.top().first;
        int u = pq.top().second;
        pq.pop();

        if (d > dist[u]) continue;
        if (u == endId) break;

        if (adj.contains(u)) {
            for (const auto& edge : adj[u]) {
                double weight = getEdgeWeight(edge, mode);
                if (dist[u] + weight < dist[edge.v]) {
                    dist[edge.v] = dist[u] + weight;
                    parent[edge.v] = u;
                    pq.push({dist[edge.v], edge.v});
                }
            }
        }
    }

    if (dist[endId] == std::numeric_limits<double>::max()) return path;

    int curr = endId;
    while (curr != startId) {
        path.prepend(curr);
        curr = parent[curr];
    }
    path.prepend(startId);
    return path;
}

QVector<int> GraphModel::findPath(int startId, int endId) {
    return findPathWithMode(startId, endId, WeightMode::DISTANCE);
}

QVector<PathRecommendation> GraphModel::recommendPaths(int startId, int endId) {
    QVector<PathRecommendation> recs;
    
    // 1. 最短路径
    QVector<int> p1 = findPathWithMode(startId, endId, WeightMode::DISTANCE);
    if (!p1.isEmpty()) {
        recs.append(PathRecommendation(RouteType::SHORTEST, "经济适用", "最短路", p1, 
            calculateDistance(p1), calculateDuration(p1), calculateCost(p1)));
    }
    
    // 2. 最快路径 (Time)
    QVector<int> p2 = findPathWithMode(startId, endId, WeightMode::TIME);
    if (!p2.isEmpty() && p2 != p1) {
        recs.append(PathRecommendation(RouteType::FASTEST, "极限冲刺", "最快路", p2,
            calculateDistance(p2), calculateDuration(p2), calculateCost(p2)));
    }

    return recs;
}

double GraphModel::calculateDistance(const QVector<int>& pathNodeIds) const {
    double total = 0;
    for(int i=0; i<pathNodeIds.size()-1; ++i) {
        const Edge* e = findEdge(pathNodeIds[i], pathNodeIds[i+1]);
        if(e) total += e->distance;
    }
    return total;
}

double GraphModel::calculateDuration(const QVector<int>& pathNodeIds) const {
    double total = 0;
    for(int i=0; i<pathNodeIds.size()-1; ++i) {
        const Edge* e = findEdge(pathNodeIds[i], pathNodeIds[i+1]);
        if(e) total += getEdgeWeight(*e, WeightMode::TIME);
    }
    return total;
}

double GraphModel::calculateCost(const QVector<int>& pathNodeIds) const {
    return calculateDistance(pathNodeIds); // 简化
}

// Getters & Helpers
const Edge* GraphModel::findEdge(int u, int v) const {
    for(const auto& e : edgesList) {
        if ((e.u == u && e.v == v) || (e.u == v && e.v == u)) return &e;
    }
    return nullptr;
}
Node GraphModel::getNode(int id) { return nodesMap.value(id); }
Node* GraphModel::getNodePtr(int id) { return nodesMap.contains(id) ? &nodesMap[id] : nullptr; }
QVector<Node> GraphModel::getAllNodes() const { return nodesMap.values(); }
QVector<Edge> GraphModel::getAllEdges() const { return edgesList; }