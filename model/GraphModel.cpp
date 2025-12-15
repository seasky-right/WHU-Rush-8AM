#include "GraphModel.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QStringList>
#include <queue>
#include <limits>
#include <functional>
#include <cmath>

// 常量定义
const double SPEED_WALK = 1.2;              // m/s
const double SPEED_BIKE = 4.2;              // m/s
const double SLOPE_THRESHOLD = 0.05;        // 5%
const double DOWNHILL_MULTIPLIER = 1.5;    // 下坡加速
const double UPHILL_MULTIPLIER_BIKE = 0.3; // 上坡减速（自行车）
const double UPHILL_MULTIPLIER_WALK = 0.8; // 上坡减速（步行）
const double UPHILL_COST_MULTIPLIER = 20.0; // 懒人养生：上坡权重增加
const double STAIR_COST_MULTIPLIER = 10.0;  // 懒人养生：楼梯权重增加

GraphModel::GraphModel() {}

bool GraphModel::loadData(const QString& nodesPath, const QString& edgesPath) {
    qDebug() << "[GraphModel] 正在加载数据...";
    qDebug() << "  - 节点路径:" << nodesPath;
    qDebug() << "  - 边路径:" << edgesPath;

    nodesMap.clear();
    edgesList.clear();

    QFile nodeFile(nodesPath);
    if (!nodeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "❌ 错误: 无法打开节点文件!" << nodesPath;
        return false;
    }
    QTextStream inNode(&nodeFile);
    while (!inNode.atEnd()) {
        QString line = inNode.readLine().trimmed();
        if (!line.isEmpty() && !line.startsWith("#")) parseNodeLine(line);
    }
    nodeFile.close();

    QFile edgeFile(edgesPath);
    if (!edgeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "❌ 错误: 无法打开边文件!" << edgesPath;
        return false;
    }
    QTextStream inEdge(&edgeFile);
    while (!inEdge.atEnd()) {
        QString line = inEdge.readLine().trimmed();
        if (!line.isEmpty() && !line.startsWith("#")) parseEdgeLine(line);
    }
    edgeFile.close();

    buildAdjacencyList();

    qDebug() << "✅ 数据加载成功! 节点数:" << nodesMap.size() << " 边数:" << edgesList.size();
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
    node.type = parts[5].trimmed().toInt();
    node.description = parts[6].trimmed();
    node.category = parts[7].trimmed();
    nodesMap.insert(node.id, node);
}

void GraphModel::parseEdgeLine(const QString& line) {
    QStringList parts = line.split(",");
    if (parts.size() < 7) return;
    Edge edge;
    edge.u = parts[0].trimmed().toInt();
    edge.v = parts[1].trimmed().toInt();
    edge.distance = parts[2].trimmed().toDouble();
    edge.type = parts[3].trimmed().toInt();
    edge.isSlope = (parts[4].trimmed().toInt() == 1);
    edge.name = parts[5].trimmed();
    edge.description = parts[6].trimmed();
    
    // 计算坡度：如果有高度差信息，计算坡度
    // 这里简单处理：如果是坡道（isSlope=true），坡度设为 0.08（8%）
    // 否则设为0
    if (edge.isSlope) {
        edge.slope = 0.08;  // 假设坡道的坡度为8%
    } else {
        edge.slope = 0.0;
    }
    
    edgesList.append(edge);
}

void GraphModel::buildAdjacencyList() {
    adj.clear();
    for (const auto& edge : edgesList) {
        adj[edge.u].append(edge);
        Edge revEdge = edge;
        revEdge.u = edge.v;
        revEdge.v = edge.u;
        // 反向边的坡度相反
        revEdge.slope = -edge.slope;
        adj[edge.v].append(revEdge);
    }
}

QVector<int> GraphModel::findPath(int startId, int endId) {
    return findPathWithMode(startId, endId, WeightMode::DISTANCE);
}

// 获取边对象
const Edge* GraphModel::findEdge(int u, int v) const {
    if (!adj.contains(u)) return nullptr;
    for (const auto& edge : adj.value(u)) {
        if (edge.v == v) {
            return &edgesList[(&edge - &edgesList[0])];
        }
    }
    return nullptr;
}

// 获取边权重
double GraphModel::getEdgeWeight(const Edge& edge, WeightMode mode) const {
    switch (mode) {
        case WeightMode::DISTANCE:
            return edge.distance;
            
        case WeightMode::TIME: {
            // 极限冲刺：基于时间，考虑坡度对速度的影响
            double baseSpeed = SPEED_WALK;  // 默认步行
            double effectiveSpeed = getEffectiveSpeed(baseSpeed, edge, mode);
            double duration = edge.distance / effectiveSpeed;
            return duration;
        }
        
        case WeightMode::COST: {
            // 懒人养生：基于心理代价，反感坡度和楼梯
            double cost = edge.distance;
            
            // 如果是楼梯（type=2或其他标记），增加权重
            if (edge.type == 2) {  // 假设 type=2 代表楼梯
                cost *= STAIR_COST_MULTIPLIER;
            }
            // 如果是上坡，增加权重
            else if (edge.slope > SLOPE_THRESHOLD) {
                cost *= UPHILL_COST_MULTIPLIER;
            }
            
            return cost;
        }
        
        default:
            return edge.distance;
    }
}

// 计算有效速度（考虑坡度和交通工具）
double GraphModel::getEffectiveSpeed(double baseSpeed, const Edge& edge, WeightMode mode) const {
    if (mode != WeightMode::TIME) {
        return baseSpeed;
    }
    
    // 如果是楼梯，无法使用基速度计算（简化处理）
    if (edge.type == 2) {
        return baseSpeed * 0.5;  // 楼梯速度减半
    }
    
    // 如果坡度为0或非常小，使用基础速度
    if (std::abs(edge.slope) <= 0.01) {
        return baseSpeed;
    }
    
    // 下坡：加速
    if (edge.slope < -0.01) {
        return baseSpeed * DOWNHILL_MULTIPLIER;
    }
    
    // 上坡：减速（这里以步行为例）
    if (edge.slope > SLOPE_THRESHOLD) {
        return baseSpeed * UPHILL_MULTIPLIER_WALK;
    }
    
    return baseSpeed;
}

// 带权重模式的Dijkstra寻路
QVector<int> GraphModel::findPathWithMode(int startId, int endId, WeightMode mode) {
    QVector<int> path;
    if (!nodesMap.contains(startId) || !nodesMap.contains(endId)) return path;

    QMap<int, double> dist;
    QMap<int, int> parent;
    for (int id : nodesMap.keys()) dist[id] = std::numeric_limits<double>::max();
    dist[startId] = 0;

    std::priority_queue<std::pair<double, int>,
                        std::vector<std::pair<double, int>>,
                        std::greater<std::pair<double, int>>> pq;
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
    path.prepend(curr);
    while (curr != startId) {
        if (!parent.contains(curr)) break;
        curr = parent[curr];
        path.prepend(curr);
    }
    return path;
}

// 计算路径距离
double GraphModel::calculateDistance(const QVector<int>& pathNodeIds) const {
    double total = 0.0;
    for (int i = 0; i < pathNodeIds.size() - 1; ++i) {
        int u = pathNodeIds[i];
        int v = pathNodeIds[i + 1];
        
        // 在邻接表中找到这条边
        if (adj.contains(u)) {
            for (const auto& edge : adj.value(u)) {
                if (edge.v == v) {
                    total += edge.distance;
                    break;
                }
            }
        }
    }
    return total;
}

// 计算路径耗时
double GraphModel::calculateDuration(const QVector<int>& pathNodeIds) const {
    double total = 0.0;
    for (int i = 0; i < pathNodeIds.size() - 1; ++i) {
        int u = pathNodeIds[i];
        int v = pathNodeIds[i + 1];
        
        // 在邻接表中找到这条边
        if (adj.contains(u)) {
            for (const auto& edge : adj.value(u)) {
                if (edge.v == v) {
                    double weight = getEdgeWeight(edge, WeightMode::TIME);
                    total += weight;
                    break;
                }
            }
        }
    }
    return total;
}

// 计算路径心理代价
double GraphModel::calculateCost(const QVector<int>& pathNodeIds) const {
    double total = 0.0;
    for (int i = 0; i < pathNodeIds.size() - 1; ++i) {
        int u = pathNodeIds[i];
        int v = pathNodeIds[i + 1];
        
        // 在邻接表中找到这条边
        if (adj.contains(u)) {
            for (const auto& edge : adj.value(u)) {
                if (edge.v == v) {
                    double weight = getEdgeWeight(edge, WeightMode::COST);
                    total += weight;
                    break;
                }
            }
        }
    }
    return total;
}

// 多策略推荐路径
QVector<PathRecommendation> GraphModel::recommendPaths(int startId, int endId) {
    QVector<PathRecommendation> recommendations;
    
    // 计算三种不同权重的路径
    QVector<int> fastestPath = findPathWithMode(startId, endId, WeightMode::TIME);
    QVector<int> easiestPath = findPathWithMode(startId, endId, WeightMode::COST);
    QVector<int> shortestPath = findPathWithMode(startId, endId, WeightMode::DISTANCE);
    
    if (fastestPath.isEmpty()) return recommendations;  // 无法寻路
    
    // 去重：如果路径相同则不重复添加
    QVector<QVector<int>> uniquePaths;
    QString routeLabels[] = {"路线1", "路线2", "路线3"};
    QString typeNames[] = {"极限冲刺", "懒人养生", "经济适用"};
    RouteType types[] = {RouteType::FASTEST, RouteType::EASIEST, RouteType::SHORTEST};
    QVector<int> paths[] = {fastestPath, easiestPath, shortestPath};
    
    for (int i = 0; i < 3; ++i) {
        // 检查这条路径是否已经存在
        bool isDuplicate = false;
        for (const auto& existing : uniquePaths) {
            if (existing == paths[i]) {
                isDuplicate = true;
                break;
            }
        }
        
        if (!isDuplicate && !paths[i].isEmpty()) {
            uniquePaths.append(paths[i]);
            
            double distance = calculateDistance(paths[i]);
            double duration = calculateDuration(paths[i]);
            double cost = calculateCost(paths[i]);
            
            PathRecommendation rec(types[i], typeNames[i], 
                                 QString("路线%1").arg(recommendations.size() + 1),
                                 paths[i], distance, duration, cost);
            recommendations.append(rec);
        }
    }
    
    return recommendations;
}

Node GraphModel::getNode(int id) { return nodesMap.value(id); }
QVector<Node> GraphModel::getAllNodes() const { return nodesMap.values(); }
QVector<Edge> GraphModel::getAllEdges() const { return edgesList; }
