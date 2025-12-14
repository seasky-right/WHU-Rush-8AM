#include "GraphModel.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QStringList>
#include <queue>
#include <limits>
#include <functional>

GraphModel::GraphModel() {}

bool GraphModel::loadData(const QString& nodesPath, const QString& edgesPath) {
    qDebug() << "[GraphModel] 正在加载数据...";
    qDebug() << "  - 节点路径:" << nodesPath;
    qDebug() << "  - 边路径:" << edgesPath;

    nodesMap.clear();
    edgesList.clear();

    // 1. 加载节点
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

    // 2. 加载边
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

    // 3. 构建邻接表
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
    edgesList.append(edge);
}

void GraphModel::buildAdjacencyList() {
    adj.clear();
    for (const auto& edge : edgesList) {
        adj[edge.u].append(edge);
        Edge revEdge = edge;
        revEdge.u = edge.v;
        revEdge.v = edge.u;
        adj[edge.v].append(revEdge);
    }
}

QVector<int> GraphModel::findPath(int startId, int endId) {
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
                if (dist[u] + edge.distance < dist[edge.v]) {
                    dist[edge.v] = dist[u] + edge.distance;
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

Node GraphModel::getNode(int id) { return nodesMap.value(id); }
QVector<Node> GraphModel::getAllNodes() const { return nodesMap.values(); }
QVector<Edge> GraphModel::getAllEdges() const { return edgesList; }
