#include "GraphModel.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <queue>
#include <limits>
#include <cmath>
#include <algorithm>
#include <QStringConverter>

// =========================================================
//  构造与 IO 部分
// =========================================================

GraphModel::GraphModel() {
    maxBuildingId = 100;
    maxRoadId = 10000;
}

bool GraphModel::loadData(const QString& nodesPath, const QString& edgesPath) {
    m_nodesPath = nodesPath;
    m_edgesPath = edgesPath;

    nodesMap.clear();
    edgesList.clear();
    maxBuildingId = 100; 
    maxRoadId = 10000;

    // 1. 加载节点
    QFile nodeFile(nodesPath);
    if (nodeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&nodeFile);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            parseNodeLine(line);
        }
        nodeFile.close();
    } else {
        qDebug() << "⚠️ Warning: 无法打开节点文件 (可能是首次运行):" << nodesPath;
    }

    // 2. 加载边
    QFile edgeFile(edgesPath);
    if (edgeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&edgeFile);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            parseEdgeLine(line);
        }
        edgeFile.close();
    }

    // 3. 构建邻接表
    buildAdjacencyList();
    
    // 【关键修复】加载完成后，再次校准 ID 计数器，确保比现有的都大
    for (auto it = nodesMap.begin(); it != nodesMap.end(); ++it) {
        int id = it.key();
        if (it.value().type == NodeType::Visible) {
            if (id >= maxBuildingId) maxBuildingId = id + 1;
        } else {
            if (id >= maxRoadId) maxRoadId = id + 1;
        }
    }

    qDebug() << "✅ 数据加载完毕: Nodes=" << nodesMap.size() << " Edges=" << edgesList.size();
    return true;
}

// 加载时刻表
bool GraphModel::loadSchedule(const QString& csvPath) {
    stationSchedules.clear();
    QFile file(csvPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            parseScheduleLine(line);
        }
        file.close();
        qDebug() << "✅ 时刻表加载完毕: 包含" << stationSchedules.size() << "个站点数据";
        return true;
    } else {
        qDebug() << "⚠️ Warning: 无法打开时刻表文件:" << csvPath;
        return false;
    }
}

bool GraphModel::saveData(const QString& nodesPath, const QString& edgesPath) {
    // 【关键修复】确保目录存在
    QFileInfo fileInfo(nodesPath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 保存节点
    QFile nodeFile(nodesPath);
    if (!nodeFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "❌ Error: 无法写入节点文件:" << nodesPath;
        return false;
    }
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
    if (!edgeFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "❌ Error: 无法写入边文件:" << edgesPath;
        return false;
    }
    QTextStream outEdge(&edgeFile);
    outEdge.setEncoding(QStringConverter::Utf8);
    for(const auto& e : edgesList) {
        outEdge << e.u << "," << e.v << "," << e.distance << ","
                << static_cast<int>(e.type) << "," << e.slope << ","
                << e.name << "," << e.description << "\n";
    }
    edgeFile.close();
    return true;
}

void GraphModel::parseNodeLine(const QString& line) {
    if (line.isEmpty() || line.startsWith("#")) return;
    QStringList parts = line.split(",");
    if (parts.size() < 6) return;
    
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
}

void GraphModel::parseEdgeLine(const QString& line) {
    if (line.isEmpty() || line.startsWith("#")) return;
    QStringList parts = line.split(",");
    if (parts.size() < 3) return;

    Edge edge;
    edge.u = parts[0].toInt();
    edge.v = parts[1].toInt();
    edge.distance = parts[2].toDouble();
    
    if (parts.size() > 3) edge.type = static_cast<EdgeType>(parts[3].toInt());
    else edge.type = EdgeType::Normal;

    if (parts.size() > 4) edge.slope = parts[4].toDouble();
    else edge.slope = 0.0;
    
    if (parts.size() > 5) edge.name = parts[5].trimmed();
    if (parts.size() > 6) edge.description = parts[6].trimmed();
    
    edgesList.append(edge);
}

void GraphModel::parseScheduleLine(const QString& line) {
    if (line.isEmpty() || line.startsWith("#")) return;
    QStringList parts = line.split(",");
    if (parts.size() < 2) return;

    int stationId = parts[0].toInt();
    QVector<QTime> times;
    for (int i = 1; i < parts.size(); ++i) {
        QString tStr = parts[i].trimmed();
        QTime t = QTime::fromString(tStr, "H:mm");
        if (!t.isValid()) t = QTime::fromString(tStr, "HH:mm");
        if (t.isValid()) times.append(t);
    }
    std::sort(times.begin(), times.end());
    stationSchedules.insert(stationId, times);
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

// =========================================================
//  编辑器 CRUD 功能
// =========================================================

int GraphModel::addNode(double x, double y, NodeType type) {
    // 1. 获取对应的计数器指针
    int* pCounter = (type == NodeType::Visible) ? &maxBuildingId : &maxRoadId;

    // 2. 【关键修复】强制递增直到找到空闲 ID
    while (nodesMap.contains(*pCounter)) {
        (*pCounter)++;
    }

    // 3. 取出当前可用 ID
    int id = (*pCounter)++;

    Node n;
    n.id = id;
    
    if (type == NodeType::Visible) {
        n.name = QString("建筑_%1").arg(id);
        n.category = NodeCategory::None;
    } else {
        n.name = QString("路口_%1").arg(id);
        n.category = NodeCategory::Road; 
    }

    n.x = x; n.y = y; n.z = 30.0;
    n.type = type;
    n.description = "无";
    
    nodesMap.insert(id, n);
    
    HistoryAction act;
    act.type = HistoryAction::AddNode;
    act.nodeData = n;
    undoStack.push(act);
    
    autoSave();
    return id;
}

void GraphModel::deleteNode(int id) {
    if (!nodesMap.contains(id)) return;
    Node target = nodesMap[id];
    
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
    undoStack.push(act);

    autoSave();
}

void GraphModel::updateNode(const Node& n) {
    if (nodesMap.contains(n.id)) {
        nodesMap[n.id] = n;
        autoSave();
    }
}

void GraphModel::addOrUpdateEdge(const Edge& edge) {
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
    autoSave();
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
            
            autoSave();
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
        break;
    case HistoryAction::DeleteNode:
        nodesMap.insert(act.nodeData.id, act.nodeData);
        break;
    case HistoryAction::AddEdge:
        deleteEdge(act.edgeData.u, act.edgeData.v); 
        undoStack.pop(); 
        break;
    case HistoryAction::DeleteEdge:
        edgesList.append(act.edgeData);
        buildAdjacencyList();
        break;
    case HistoryAction::MoveNode:
        nodesMap[act.nodeData.id] = act.nodeData; 
        break;
    }
    autoSave();
}

bool GraphModel::canUndo() const { return !undoStack.isEmpty(); }

void GraphModel::pushAction(const HistoryAction& action) {
    undoStack.push(action);
}

// =========================================================
//  核心物理与寻路逻辑
// =========================================================

double GraphModel::getRealSpeed(TransportMode mode, Weather weather) const {
    double speed = 0.0;
    switch(mode) {
        case TransportMode::Walk:
            speed = Config::SPEED_WALK;
            if (weather == Weather::Rainy) speed *= 0.8; 
            if (weather == Weather::Snowy) speed *= 0.6; 
            break;
        case TransportMode::Run:
            speed = Config::SPEED_RUN;
            if (weather != Weather::Sunny) speed *= 0.7; 
            break;
        case TransportMode::SharedBike:
            if (weather == Weather::Snowy) return 0.0001; 
            speed = Config::SPEED_SHARED_BIKE;
            break;
        case TransportMode::EBike:
            if (weather == Weather::Snowy) return 0.0001; 
            speed = Config::SPEED_EBIKE;
            break;
        case TransportMode::Bus:
            speed = Config::SPEED_BUS;
            break;
    }
    return speed;
}

double GraphModel::getEdgeWeight(const Edge& edge, WeightMode weightMode, 
                               TransportMode transportMode, Weather weather) const {
    bool isVehicle = (transportMode == TransportMode::SharedBike || 
                      transportMode == TransportMode::EBike);
    
    if (weather == Weather::Snowy && isVehicle) return std::numeric_limits<double>::max();
    if (isVehicle && (edge.type == EdgeType::Stairs || edge.type == EdgeType::Indoor)) return std::numeric_limits<double>::max();
    if (weightMode == WeightMode::DISTANCE) return edge.distance;

    double speed = getRealSpeed(transportMode, weather);
    if (std::abs(edge.slope) > Config::SLOPE_THRESHOLD) {
        if (transportMode == TransportMode::SharedBike) speed *= 0.3; 
        else if (transportMode == TransportMode::Walk) speed *= 0.8;  
        else if (transportMode == TransportMode::Run) speed *= 0.6;   
    }

    double penaltyMultiplier = 1.0;
    if (weather == Weather::Rainy && isVehicle) penaltyMultiplier = 1.5;

    double time = (edge.distance / speed) * penaltyMultiplier;
    if (weightMode == WeightMode::TIME) return time;

    if (weightMode == WeightMode::COST) {
        double cost = edge.distance;
        if (std::abs(edge.slope) > Config::SLOPE_THRESHOLD) cost *= 20.0; 
        if (edge.type == EdgeType::Stairs) cost *= 10.0; 
        if (weather == Weather::Snowy && edge.type == EdgeType::Stairs) cost *= 100.0;
        return cost;
    }
    return edge.distance;
}

QVector<int> GraphModel::findPath(int startId, int endId, TransportMode mode, Weather weather) {
    if (!nodesMap.contains(startId) || !nodesMap.contains(endId)) return {};

    QMap<int, double> dist;
    QMap<int, int> parent;
    for(int id : nodesMap.keys()) dist[id] = std::numeric_limits<double>::max();
    
    std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<>> pq;
    dist[startId] = 0;
    pq.push({0, startId});

    while (!pq.empty()) {
        double d = pq.top().first;
        int u = pq.top().second;
        pq.pop();
        if (d > dist[u]) continue;
        if (u == endId) break;
        if (!adj.contains(u)) continue;

        for (const Edge& e : adj[u]) {
            double weight = getEdgeWeight(e, WeightMode::TIME, mode, weather);
            if (weight >= std::numeric_limits<double>::max()) continue;
            if (dist[u] + weight < dist[e.v]) {
                dist[e.v] = dist[u] + weight;
                parent[e.v] = u;
                pq.push({dist[e.v], e.v});
            }
        }
    }

    QVector<int> path;
    if (dist[endId] == std::numeric_limits<double>::max()) return path;
    int curr = endId;
    while (curr != startId) {
        path.append(curr);
        curr = parent[curr];
    }
    path.append(startId);
    std::reverse(path.begin(), path.end());
    return path;
}

// 校车逻辑
QTime GraphModel::getNextBusTime(int stationId, QTime arrivalTime, Weather weather) const {
    if (!stationSchedules.contains(stationId)) return QTime();
    int delayMinutes = 0;
    if (weather == Weather::Rainy) delayMinutes = 5;
    if (weather == Weather::Snowy) delayMinutes = 15;

    const QVector<QTime>& rawTimes = stationSchedules[stationId];
    for (const QTime& rawT : rawTimes) {
        QTime effectiveT = rawT.addSecs(delayMinutes * 60);
        if (effectiveT >= arrivalTime) return effectiveT;
    }
    return QTime();
}

GraphModel::BusRouteResult GraphModel::calculateBestBusRoute(int startId, int endId, QTime currentTime, Weather weather) {
    BusRouteResult bestResult;
    bestResult.valid = false;
    bestResult.totalDuration = std::numeric_limits<double>::max();

    QVector<int> stations;
    for (auto it = nodesMap.begin(); it != nodesMap.end(); ++it) {
        if (it.value().category == NodeCategory::BusStation) stations.append(it.key());
    }
    if (stations.isEmpty()) return bestResult;

    for (int startStation : stations) {
        QVector<int> walk1Path = findPath(startId, startStation, TransportMode::Walk, weather);
        if (walk1Path.isEmpty()) continue;
        double walk1Time = calculateDuration(walk1Path, TransportMode::Walk, weather);
        QTime arrivalAtStation = currentTime.addSecs((int)walk1Time);
        QTime busTime = getNextBusTime(startStation, arrivalAtStation, weather);
        if (!busTime.isValid()) continue;

        double waitTime = arrivalAtStation.secsTo(busTime);

        for (int endStation : stations) {
            if (startStation == endStation) continue;
            QVector<int> ridePath = findPath(startStation, endStation, TransportMode::Bus, weather);
            if (ridePath.isEmpty()) continue;
            double rideTime = calculateDuration(ridePath, TransportMode::Bus, weather);
            QVector<int> walk2Path = findPath(endStation, endId, TransportMode::Walk, weather);
            if (walk2Path.isEmpty()) continue;
            double walk2Time = calculateDuration(walk2Path, TransportMode::Walk, weather);

            double total = walk1Time + waitTime + rideTime + walk2Time;
            if (total < bestResult.totalDuration) {
                bestResult.valid = true;
                bestResult.totalDuration = total;
                bestResult.stationStartId = startStation;
                bestResult.stationEndId = endStation;
                bestResult.nextBusTime = busTime;
                bestResult.fullPath = walk1Path;
                if (!ridePath.isEmpty()) {
                    for (int k = 1; k < ridePath.size(); ++k) bestResult.fullPath.append(ridePath[k]);
                }
                if (!walk2Path.isEmpty()) {
                    for (int k = 1; k < walk2Path.size(); ++k) bestResult.fullPath.append(walk2Path[k]);
                }
            }
        }
    }
    return bestResult;
}

bool GraphModel::isLate(double durationSeconds, QTime current, QTime target) const {
    QTime arrival = current.addSecs((int)durationSeconds);
    return arrival > target;
}

PathRecommendation GraphModel::getSpecificRoute(int startId, int endId, 
                                              TransportMode mode,
                                              Weather weather, 
                                              QTime currentTime, 
                                              QTime classTime,
                                              bool enableLateCheck) {
    if (mode == TransportMode::Bus) {
        BusRouteResult busRes = calculateBestBusRoute(startId, endId, currentTime, weather);
        if (!busRes.valid) return PathRecommendation();
        
        bool late = enableLateCheck && isLate(busRes.totalDuration, currentTime, classTime);
        double dist = calculateDistance(busRes.fullPath);
        return PathRecommendation(RouteType::FASTEST, "校车通勤", 
            QString("班次 %1").arg(busRes.nextBusTime.toString("HH:mm")), 
            busRes.fullPath, dist, busRes.totalDuration, 0, late);
    }

    QVector<int> path = findPath(startId, endId, mode, weather);
    if (path.isEmpty()) return PathRecommendation();

    double dist = calculateDistance(path);
    double totalTime = calculateDuration(path, mode, weather);
    if (mode == TransportMode::SharedBike) totalTime += Config::TIME_FIND_BIKE + Config::TIME_PARK_BIKE;

    QString typeName = "普通模式";
    QString label = "推荐";
    switch (mode) {
        case TransportMode::Walk: typeName = "步行"; label = "稳健保底"; break;
        case TransportMode::SharedBike: typeName = "共享单车"; label = "随停随取"; break;
        case TransportMode::EBike: typeName = "私人电驴"; label = "速度王者"; break;
        case TransportMode::Run: typeName = "极限狂奔"; label = "可能会累"; break;
        default: break;
    }

    bool late = enableLateCheck && isLate(totalTime, currentTime, classTime);
    return PathRecommendation(RouteType::FASTEST, typeName, label, path, dist, totalTime, 0, late);
}

double GraphModel::calculateDuration(const QVector<int>& pathNodeIds, TransportMode mode, Weather weather) const {
    double total = 0;
    for(int i=0; i<pathNodeIds.size()-1; ++i) {
        const Edge* e = findEdge(pathNodeIds[i], pathNodeIds[i+1]);
        if(e) total += getEdgeWeight(*e, WeightMode::TIME, mode, weather);
    }
    return total;
}

double GraphModel::calculateDistance(const QVector<int>& pathNodeIds) const {
    double total = 0;
    for(int i=0; i<pathNodeIds.size()-1; ++i) {
        const Edge* e = findEdge(pathNodeIds[i], pathNodeIds[i+1]);
        if(e) total += e->distance;
    }
    return total;
}

double GraphModel::calculateCost(const QVector<int>& pathNodeIds) const {
    return calculateDistance(pathNodeIds); 
}

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