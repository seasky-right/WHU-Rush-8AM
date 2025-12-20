#include "GraphModel.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
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
    // 【新增】记住路径，方便后续 autoSave 使用
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
        qDebug() << "❌ Error: 无法打开节点文件:" << nodesPath;
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

    // 【重要】加载数据时，必须更新 maxBuildingId 和 maxRoadId
    // 确保它们比文件中现有的 ID 都要大
    if (node.type == NodeType::Visible) {
        if (node.id >= maxBuildingId) maxBuildingId = node.id + 1;
    } 
    else if (node.type == NodeType::Ghost) { // 路口
        if (node.id >= maxRoadId) maxRoadId = node.id + 1;
    }
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

    // 2. 【修复】安全查找空闲 ID (防止死循环)
    int loopSafetyLimit = 100000; // 熔断阈值
    int loopCount = 0;
    
    while (nodesMap.contains(*pCounter)) {
        (*pCounter)++;
        loopCount++;
        if (loopCount > loopSafetyLimit) {
            qDebug() << "❌ 严重错误: ID 生成器陷入死循环，强制中断。当前 ID:" << *pCounter;
            break; 
        }
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
    
    // 4. 执行自动保存
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

    // 【新增】立即保存
    autoSave();
}

void GraphModel::updateNode(const Node& n) {
    if (nodesMap.contains(n.id)) {
        nodesMap[n.id] = n;
        // 【新增】立即保存
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
    
    // 【新增】立即保存
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
            
            // 【新增】立即保存
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
        undoStack.pop(); // 因为 deleteEdge 内部又 push 了一次，需要弹出来防止重复
        break;
    case HistoryAction::DeleteEdge:
        edgesList.append(act.edgeData);
        buildAdjacencyList();
        break;
    case HistoryAction::MoveNode:
        nodesMap[act.nodeData.id] = act.nodeData; 
        break;
    }
    
    // 【新增】立即保存
    autoSave();
}

bool GraphModel::canUndo() const { return !undoStack.isEmpty(); }

void GraphModel::pushAction(const HistoryAction& action) {
    undoStack.push(action);
}

// =========================================================
//  核心物理与寻路逻辑 (物理引擎 + 单模式路线生成)
// =========================================================

double GraphModel::getRealSpeed(TransportMode mode, Weather weather) const {
    double speed = 0.0;
    
    switch(mode) {
        case TransportMode::Walk:
            speed = Config::SPEED_WALK;
            if (weather == Weather::Rainy) speed *= 0.8; // 雨天 -20%
            if (weather == Weather::Snowy) speed *= 0.6; // 雪天 -40%
            break;
        case TransportMode::Run:
            speed = Config::SPEED_RUN;
            if (weather != Weather::Sunny) speed *= 0.7; // 恶劣天气跑步大打折扣
            break;
        case TransportMode::SharedBike:
            // 雪天物理禁止骑行，返回极小值防止除零，逻辑层会处理无穷大权重
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
    
    // === 1. 通行性检查 (Accessibility) ===
    bool isVehicle = (transportMode == TransportMode::SharedBike || 
                      transportMode == TransportMode::EBike);
    
    // [规则] 雪天禁用自行车/电瓶车
    if (weather == Weather::Snowy && isVehicle) {
        return std::numeric_limits<double>::max();
    }

    // [规则] 车辆不可走楼梯或室内
    if (isVehicle && (edge.type == EdgeType::Stairs || edge.type == EdgeType::Indoor)) {
        return std::numeric_limits<double>::max();
    }

    // === 2. 基础属性 ===
    if (weightMode == WeightMode::DISTANCE) return edge.distance;

    // === 3. 时间计算 (核心物理引擎) ===
    double speed = getRealSpeed(transportMode, weather);
    
    // [规则] 坡度逻辑 (Slope Physics)
    // 只要有坡度且超过阈值，就进行减速
    if (std::abs(edge.slope) > Config::SLOPE_THRESHOLD) {
        if (transportMode == TransportMode::SharedBike) speed *= 0.3; // 骑车上坡极慢
        else if (transportMode == TransportMode::Walk) speed *= 0.8;  // 走路微慢
        else if (transportMode == TransportMode::Run) speed *= 0.6;   // 跑步上坡很累
        // 电驴动力强，这里设定为不减速
    }

    // [规则] 雨天骑行安全惩罚 (PRD: 权重 x1.5，模拟小心翼翼)
    double penaltyMultiplier = 1.0;
    if (weather == Weather::Rainy && isVehicle) {
        penaltyMultiplier = 1.5;
    }

    double time = (edge.distance / speed) * penaltyMultiplier;

    if (weightMode == WeightMode::TIME) return time;

    // === 4. 心理代价 (Cost / 懒人指数) ===
    if (weightMode == WeightMode::COST) {
        double cost = edge.distance;
        
        // 懒人核心：极度排斥上坡和楼梯
        if (std::abs(edge.slope) > Config::SLOPE_THRESHOLD) cost *= 20.0; // 极度厌恶上坡
        if (edge.type == EdgeType::Stairs) cost *= 10.0; // 厌恶楼梯
        
        // 雪天走楼梯极其危险
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
    
    WeightMode obj = WeightMode::TIME; 
    
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
            // [关键] 传入 weather 和 mode
            double weight = getEdgeWeight(e, obj, mode, weather);
            
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

bool GraphModel::isLate(double durationSeconds, QTime current, QTime target) const {
    QTime arrival = current.addSecs((int)durationSeconds);
    return arrival > target;
}

PathRecommendation GraphModel::getSpecificRoute(int startId, int endId, 
                                              TransportMode mode,
                                              Weather weather, 
                                              QTime currentTime, 
                                              QTime classTime,
                                              bool enableLateCheck) { // <--- 新增参数
    // 1. 基础寻路
    QVector<int> path = findPath(startId, endId, mode, weather);
    
    // 如果无路可走
    if (path.isEmpty()) return PathRecommendation(); 

    // 2. 计算基础数据
    double dist = calculateDistance(path);
    double pureTime = calculateDuration(path, mode, weather);
    double totalTime = pureTime;
    
    // 3. 应用模式特定的逻辑 (Context Logic)
    QString typeName;
    QString label;
    RouteType rType = RouteType::FASTEST;

    switch (mode) {
    case TransportMode::Walk:
        typeName = "步行";
        label = "稳健保底";
        rType = RouteType::SHORTEST;
        break;

    case TransportMode::SharedBike:
        typeName = "共享单车";
        label = "随停随取";
        // 加上找车和还车时间
        totalTime += Config::TIME_FIND_BIKE + Config::TIME_PARK_BIKE;
        break;

    case TransportMode::EBike:
        typeName = "私人电驴";
        label = "速度王者";
        break;

    case TransportMode::Run:
        typeName = "极限狂奔";
        label = "可能会累";
        break;

    case TransportMode::Bus:
        typeName = "校车";
        label = "定时班车";
        break;
    }

    // 4. [核心修改] 迟到判定：只有当开关开启时才计算
    bool late = false;
    if (enableLateCheck) {
        late = isLate(totalTime, currentTime, classTime);
    }

    return PathRecommendation(rType, typeName, label, path, dist, totalTime, 0, late);
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