// ============================================================
// GraphModel.cpp - 图数据模型（核心算法文件）
// 
// 这个文件负责：
// 1. 加载和保存地图数据（节点和道路）
// 2. 实现 Dijkstra 最短路径算法
// 3. 处理不同天气、不同交通方式的路径规划
// ============================================================

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

// ============================================================
// 构造函数
// ============================================================
GraphModel::GraphModel()
{
    // 建筑ID从100开始编号
    maxBuildingId = 100;
    // 路口ID从10000开始编号（避免与建筑ID冲突）
    maxRoadId = 10000;
}

// ============================================================
// 加载地图数据（节点和道路）
// 参数：nodesPath - 节点文件路径，edgesPath - 道路文件路径
// 返回：是否加载成功
// ============================================================
bool GraphModel::loadData(const QString& nodesPath, const QString& edgesPath)
{
    // 保存文件路径，方便后续自动保存
    m_nodesPath = nodesPath;
    m_edgesPath = edgesPath;

    // 清空旧数据
    nodesMap.clear();
    edgesList.clear();
    maxBuildingId = 100;
    maxRoadId = 10000;

    // ---- 第1步：加载节点文件 ----
    QFile nodeFile(nodesPath);
    bool nodeFileOpened = nodeFile.open(QIODevice::ReadOnly | QIODevice::Text);
    
    if (nodeFileOpened)
    {
        QTextStream in(&nodeFile);
        while (!in.atEnd())
        {
            QString line = in.readLine().trimmed();
            parseNodeLine(line);  // 解析每一行
        }
        nodeFile.close();
    }
    else
    {
        qDebug() << "警告: 无法打开节点文件:" << nodesPath;
    }

    // ---- 第2步：加载道路文件 ----
    QFile edgeFile(edgesPath);
    bool edgeFileOpened = edgeFile.open(QIODevice::ReadOnly | QIODevice::Text);
    
    if (edgeFileOpened)
    {
        QTextStream in(&edgeFile);
        while (!in.atEnd())
        {
            QString line = in.readLine().trimmed();
            parseEdgeLine(line);  // 解析每一行
        }
        edgeFile.close();
    }

    // ---- 第3步：构建邻接表（用于后续寻路） ----
    buildAdjacencyList();
    
    // ---- 第4步：校准ID计数器 ----
    // 确保新建节点的ID不会与已有节点冲突
    for (auto it = nodesMap.begin(); it != nodesMap.end(); ++it)
    {
        int id = it.key();
        
        if (it.value().type == NodeType::Visible)
        {
            // 可见节点（建筑）
            if (id >= maxBuildingId)
            {
                maxBuildingId = id + 1;
            }
        }
        else
        {
            // 隐形节点（路口）
            if (id >= maxRoadId)
            {
                maxRoadId = id + 1;
            }
        }
    }

    qDebug() << "数据加载完毕: 节点数=" << nodesMap.size() << " 道路数=" << edgesList.size();
    return true;
}

// ============================================================
// 加载校车时刻表
// ============================================================
bool GraphModel::loadSchedule(const QString& csvPath)
{
    stationSchedules.clear();
    
    QFile file(csvPath);
    bool opened = file.open(QIODevice::ReadOnly | QIODevice::Text);
    
    if (opened)
    {
        QTextStream in(&file);
        while (!in.atEnd())
        {
            QString line = in.readLine().trimmed();
            parseScheduleLine(line);
        }
        file.close();
        qDebug() << "时刻表加载完毕: 包含" << stationSchedules.size() << "个站点";
        return true;
    }
    else
    {
        qDebug() << "警告: 无法打开时刻表文件:" << csvPath;
        return false;
    }
}

// ============================================================
// 保存地图数据到文件
// ============================================================
bool GraphModel::saveData(const QString& nodesPath, const QString& edgesPath)
{
    // 确保目录存在
    QFileInfo fileInfo(nodesPath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists())
    {
        dir.mkpath(".");
    }

    // ---- 保存节点 ----
    QFile nodeFile(nodesPath);
    bool nodeOpened = nodeFile.open(QIODevice::WriteOnly | QIODevice::Text);
    
    if (!nodeOpened)
    {
        qDebug() << "错误: 无法写入节点文件:" << nodesPath;
        return false;
    }
    
    QTextStream outNode(&nodeFile);
    outNode.setEncoding(QStringConverter::Utf8);
    
    // 按ID排序后写入
    QList<int> keys = nodesMap.keys();
    std::sort(keys.begin(), keys.end());
    
    for (int id : keys)
    {
        const Node& n = nodesMap[id];
        int typeInt = (n.type == NodeType::Visible) ? 0 : 9;
        QString catStr = Node::categoryToString(n.category);
        
        outNode << n.id << "," << n.name << "," << n.x << "," << n.y << "," << n.z << ","
                << typeInt << "," << n.description << "," << catStr << "\n";
    }
    nodeFile.close();

    // ---- 保存道路 ----
    QFile edgeFile(edgesPath);
    bool edgeOpened = edgeFile.open(QIODevice::WriteOnly | QIODevice::Text);
    
    if (!edgeOpened)
    {
        qDebug() << "错误: 无法写入道路文件:" << edgesPath;
        return false;
    }
    
    QTextStream outEdge(&edgeFile);
    outEdge.setEncoding(QStringConverter::Utf8);
    
    for (const Edge& e : edgesList)
    {
        outEdge << e.u << "," << e.v << "," << e.distance << ","
                << static_cast<int>(e.type) << "," << e.slope << ","
                << e.name << "," << e.description << "\n";
    }
    edgeFile.close();
    
    return true;
}

// ============================================================
// 解析节点文件的一行数据
// 格式: id, name, x, y, z, type, description, category
// ============================================================
void GraphModel::parseNodeLine(const QString& line)
{
    // 跳过空行和注释行
    if (line.isEmpty() || line.startsWith("#"))
    {
        return;
    }
    
    // 按逗号分割
    QStringList parts = line.split(",");
    if (parts.size() < 6)
    {
        return;  // 数据不完整，跳过
    }
    
    // 填充节点数据
    Node node;
    node.id = parts[0].toInt();
    node.name = parts[1].trimmed();
    node.x = parts[2].toDouble();
    node.y = parts[3].toDouble();
    node.z = parts[4].toDouble();
    
    int typeInt = parts[5].toInt();
    if (typeInt == 9)
    {
        node.type = NodeType::Ghost;  // 隐形路口
    }
    else
    {
        node.type = NodeType::Visible;  // 可见建筑
    }
    
    // 可选字段
    if (parts.size() > 7)
    {
        node.description = parts[6].trimmed();
        node.category = Node::stringToCategory(parts[7].trimmed());
    }
    else
    {
        node.description = "无";
        node.category = NodeCategory::None;
    }
    
    // 存入哈希表
    nodesMap.insert(node.id, node);
}

// ============================================================
// 解析道路文件的一行数据
// 格式: u, v, distance, type, slope, name, description
// ============================================================
void GraphModel::parseEdgeLine(const QString& line)
{
    if (line.isEmpty() || line.startsWith("#"))
    {
        return;
    }
    
    QStringList parts = line.split(",");
    if (parts.size() < 3)
    {
        return;
    }

    Edge edge;
    edge.u = parts[0].toInt();          // 起点ID
    edge.v = parts[1].toInt();          // 终点ID
    edge.distance = parts[2].toDouble(); // 距离（米）
    
    // 可选字段
    if (parts.size() > 3)
    {
        edge.type = static_cast<EdgeType>(parts[3].toInt());
    }
    else
    {
        edge.type = EdgeType::Normal;
    }

    if (parts.size() > 4)
    {
        edge.slope = parts[4].toDouble();  // 坡度
    }
    else
    {
        edge.slope = 0.0;
    }
    
    if (parts.size() > 5)
    {
        edge.name = parts[5].trimmed();
    }
    if (parts.size() > 6)
    {
        edge.description = parts[6].trimmed();
    }
    
    edgesList.append(edge);
}

// ============================================================
// 解析时刻表的一行数据
// 格式: stationId, time1, time2, time3, ...
// ============================================================
void GraphModel::parseScheduleLine(const QString& line)
{
    if (line.isEmpty() || line.startsWith("#"))
    {
        return;
    }
    
    QStringList parts = line.split(",");
    if (parts.size() < 2)
    {
        return;
    }

    int stationId = parts[0].toInt();
    QVector<QTime> times;
    
    // 从第2列开始都是发车时间
    for (int i = 1; i < parts.size(); ++i)
    {
        QString tStr = parts[i].trimmed();
        
        // 尝试解析时间格式
        QTime t = QTime::fromString(tStr, "H:mm");
        if (!t.isValid())
        {
            t = QTime::fromString(tStr, "HH:mm");
        }
        
        if (t.isValid())
        {
            times.append(t);
        }
    }
    
    // 按时间排序
    std::sort(times.begin(), times.end());
    stationSchedules.insert(stationId, times);
}

// ============================================================
// 构建邻接表（用于图搜索算法）
// 邻接表是一种存储图的方式，每个节点存储它相邻的边
// ============================================================
void GraphModel::buildAdjacencyList()
{
    adj.clear();
    
    // 遍历所有边，为每个节点建立"邻居列表"
    for (const Edge& edge : edgesList)
    {
        // 正向：从u到v
        adj[edge.u].append(edge);
        
        // 反向：从v到u（无向图需要双向）
        Edge reverseEdge = edge;
        std::swap(reverseEdge.u, reverseEdge.v);
        reverseEdge.slope = -edge.slope;  // 坡度取反
        adj[edge.v].append(reverseEdge);
    }
}

// ============================================================
//                    编辑器 CRUD 功能
// ============================================================

// ============================================================
// 添加新节点
// 参数：x,y - 坐标，type - 节点类型
// 返回：新节点的ID
// ============================================================
int GraphModel::addNode(double x, double y, NodeType type)
{
    // 根据类型选择对应的ID计数器
    int* pCounter = NULL;
    if (type == NodeType::Visible)
    {
        pCounter = &maxBuildingId;
    }
    else
    {
        pCounter = &maxRoadId;
    }

    // 找到一个没被使用的ID
    while (nodesMap.contains(*pCounter))
    {
        (*pCounter)++;
    }

    // 分配ID
    int id = *pCounter;
    (*pCounter)++;

    // 创建节点
    Node n;
    n.id = id;
    n.x = x;
    n.y = y;
    n.z = 30.0;  // 默认海拔
    n.type = type;
    n.description = "无";
    
    if (type == NodeType::Visible)
    {
        n.name = QString("建筑_%1").arg(id);
        n.category = NodeCategory::None;
    }
    else
    {
        n.name = QString("路口_%1").arg(id);
        n.category = NodeCategory::Road;
    }
    // 存入哈希表
    nodesMap.insert(id, n);
    
    // 记录操作，用于撤销
    HistoryAction act;
    act.type = HistoryAction::AddNode;
    act.nodeData = n;
    undoStack.push(act);
    
    autoSave();
    return id;
}

// ============================================================
// 删除节点
// ============================================================
void GraphModel::deleteNode(int id)
{
    if (!nodesMap.contains(id))
    {
        return;
    }
    
    Node target = nodesMap[id];
    
    // 删除与该节点相连的所有边
    for (int i = edgesList.size() - 1; i >= 0; --i)
    {
        bool connectedToTarget = (edgesList[i].u == id || edgesList[i].v == id);
        if (connectedToTarget)
        {
            edgesList.removeAt(i);
        }
    }
    
    // 删除节点
    nodesMap.remove(id);
    buildAdjacencyList();
    
    // 记录操作
    HistoryAction act;
    act.type = HistoryAction::DeleteNode;
    act.nodeData = target;
    undoStack.push(act);

    autoSave();
}

// ============================================================
// 更新节点属性
// ============================================================
void GraphModel::updateNode(const Node& n)
{
    if (nodesMap.contains(n.id))
    {
        nodesMap[n.id] = n;
        autoSave();
    }
}

// ============================================================
// 添加或更新边（道路）
// ============================================================
void GraphModel::addOrUpdateEdge(const Edge& edge)
{
    // 检查是否已存在
    bool found = false;
    for (int i = 0; i < edgesList.size(); ++i)
    {
        bool sameEdge = (edgesList[i].u == edge.u && edgesList[i].v == edge.v);
        bool reverseEdge = (edgesList[i].u == edge.v && edgesList[i].v == edge.u);
        
        if (sameEdge || reverseEdge)
        {
            // 更新已有边
            edgesList[i] = edge;
            found = true;
            break;
        }
    }
    
    if (!found)
    {
        // 添加新边
        edgesList.append(edge);
        
        HistoryAction act;
        act.type = HistoryAction::AddEdge;
        act.edgeData = edge;
        undoStack.push(act);
    }
    
    buildAdjacencyList();
    autoSave();
}

// ============================================================
// 删除边（道路）
// ============================================================
void GraphModel::deleteEdge(int u, int v)
{
    for (int i = 0; i < edgesList.size(); ++i)
    {
        const Edge& e = edgesList[i];
        bool sameEdge = (e.u == u && e.v == v);
        bool reverseEdge = (e.u == v && e.v == u);
        
        if (sameEdge || reverseEdge)
        {
            // 记录操作
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

// ============================================================
// 撤销操作
// ============================================================
void GraphModel::undo()
{
    if (undoStack.isEmpty())
    {
        return;
    }
    
    HistoryAction act = undoStack.pop();
    
    switch (act.type)
    {
    case HistoryAction::AddNode:
        // 撤销添加 = 删除
        nodesMap.remove(act.nodeData.id);
        break;
        
    case HistoryAction::DeleteNode:
        // 撤销删除 = 恢复
        nodesMap.insert(act.nodeData.id, act.nodeData);
        break;
        
    case HistoryAction::AddEdge:
        // 撤销添加边 = 删除边
        deleteEdge(act.edgeData.u, act.edgeData.v);
        undoStack.pop();  // deleteEdge会再次入栈，需要弹出
        break;
        
    case HistoryAction::DeleteEdge:
        // 撤销删除边 = 恢复边
        edgesList.append(act.edgeData);
        buildAdjacencyList();
        break;
        
    case HistoryAction::MoveNode:
        // 撤销移动 = 恢复原位置
        nodesMap[act.nodeData.id] = act.nodeData;
        break;
    }
    
    autoSave();
}

bool GraphModel::canUndo() const
{
    return !undoStack.isEmpty();
}

void GraphModel::pushAction(const HistoryAction& action)
{
    undoStack.push(action);
}

// ============================================================
//             核心物理与寻路逻辑
// ============================================================

// ============================================================
// 获取实际行进速度（米/秒）
// 根据交通方式和天气计算实际速度
// ============================================================
double GraphModel::getRealSpeed(TransportMode mode, Weather weather) const
{
    double speed = 0.0;
    
    switch (mode)
    {
    case TransportMode::Walk:
        speed = Config::SPEED_WALK;  // 基础步行速度
        if (weather == Weather::Rainy)
        {
            speed = speed * 0.8;  // 下雨减速20%
        }
        if (weather == Weather::Snowy)
        {
            speed = speed * 0.6;  // 下雪减速40%
        }
        break;
        
    case TransportMode::Run:
        speed = Config::SPEED_RUN;
        if (weather != Weather::Sunny)
        {
            speed = speed * 0.7;  // 非晴天减速30%
        }
        break;
        
    case TransportMode::SharedBike:
        if (weather == Weather::Snowy)
        {
            return 0.0001;  // 下雪不能骑车
        }
        speed = Config::SPEED_SHARED_BIKE;
        break;
        
    case TransportMode::EBike:
        if (weather == Weather::Snowy)
        {
            return 0.0001;  // 下雪不能骑电动车
        }
        speed = Config::SPEED_EBIKE;
        break;
        
    case TransportMode::Bus:
        speed = Config::SPEED_BUS;
        break;
    }
    
    return speed;
}

// ============================================================
// 计算边的权重（用于路径搜索）
// 权重可以是距离、时间或综合代价
// ============================================================
double GraphModel::getEdgeWeight(
    const Edge& edge,
    WeightMode weightMode,
    TransportMode transportMode,
    Weather weather) const
{
    // 判断是否是骑行类交通
    bool isVehicle = (transportMode == TransportMode::SharedBike || 
                      transportMode == TransportMode::EBike);
    
    // ---- 特殊情况处理：返回极大值表示"不可通行" ----
    // 下雪天不能骑车
    if (weather == Weather::Snowy && isVehicle)
    {
        return std::numeric_limits<double>::max();
    }
    
    // 骑车不能走楼梯和室内
    if (isVehicle)
    {
        bool cannotPass = (edge.type == EdgeType::Stairs || edge.type == EdgeType::Indoor);
        if (cannotPass)
        {
            return std::numeric_limits<double>::max();
        }
    }
    
    // ---- 根据权重模式计算 ----
    if (weightMode == WeightMode::DISTANCE)
    {
        return edge.distance;  // 只考虑距离
    }

    // 获取基础速度
    double speed = getRealSpeed(transportMode, weather);
    
    // 坡道减速
    if (std::abs(edge.slope) > Config::SLOPE_THRESHOLD)
    {
        if (transportMode == TransportMode::SharedBike)
        {
            speed = speed * 0.3;  // 单车爬坡很慢
        }
        else if (transportMode == TransportMode::Walk)
        {
            speed = speed * 0.8;
        }
        else if (transportMode == TransportMode::Run)
        {
            speed = speed * 0.6;
        }
    }

    // 雨天骑车额外惩罚
    double penaltyMultiplier = 1.0;
    if (weather == Weather::Rainy && isVehicle)
    {
        penaltyMultiplier = 1.5;
    }

    // 计算通过时间
    double time = (edge.distance / speed) * penaltyMultiplier;
    
    if (weightMode == WeightMode::TIME)
    {
        return time;
    }

    // 综合代价模式（懒人路线）
    if (weightMode == WeightMode::COST)
    {
        double cost = edge.distance;
        
        // 坡道很累，大幅增加代价
        if (std::abs(edge.slope) > Config::SLOPE_THRESHOLD)
        {
            cost = cost * 20.0;
        }
        
        // 楼梯也很累
        if (edge.type == EdgeType::Stairs)
        {
            cost = cost * 10.0;
        }
        
        // 下雪天走楼梯超级危险
        if (weather == Weather::Snowy && edge.type == EdgeType::Stairs)
        {
            cost = cost * 100.0;
        }
        
        return cost;
    }
    
    return edge.distance;
}

// ============================================================
// Dijkstra 最短路径算法 - 核心寻路函数
// 
// 这是计算机科学中的经典算法，用于找两点之间的最短路径
// 返回：从起点到终点的节点ID列表
// ============================================================
QVector<int> GraphModel::findPath(
    int startId,
    int endId,
    TransportMode mode,
    Weather weather,
    WeightMode weightMode)
{
    // ---- 第1步：检查起点和终点是否存在 ----
    if (!nodesMap.contains(startId) || !nodesMap.contains(endId))
    {
        return {};  // 返回空路径
    }

    // ---- 第2步：初始化距离表和父节点表 ----
    QMap<int, double> dist;  // 存储每个节点到起点的最短距离
    QMap<int, int> parent;   // 存储每个节点的前驱节点（用于回溯路径）
    
    // 所有节点初始距离设为无穷大
    for (int id : nodesMap.keys())
    {
        dist[id] = std::numeric_limits<double>::max();
    }
    
    // ---- 第3步：初始化优先队列 ----
    // 优先队列会自动按距离从小到大排序
    std::priority_queue<
        std::pair<double, int>,
        std::vector<std::pair<double, int>>,
        std::greater<>
    > pq;
    
    dist[startId] = 0;
    pq.push({0, startId});

    // ---- 第4步：Dijkstra 主循环 ----
    while (!pq.empty())
    {
        // 取出当前距离最小的节点
        double d = pq.top().first;
        int u = pq.top().second;
        pq.pop();
        
        // 如果这个距离已经过时，跳过
        if (d > dist[u])
        {
            continue;
        }
        
        // 找到终点，提前结束
        if (u == endId)
        {
            break;
        }
        
        // 如果这个节点没有邻居，跳过
        if (!adj.contains(u))
        {
            continue;
        }

        // 遍历所有相邻的边
        for (const Edge& e : adj[u])
        {
            // 计算这条边的权重
            double weight = getEdgeWeight(e, weightMode, mode, weather);
            
            // 如果这条路不通（权重为无穷大），跳过
            if (weight >= std::numeric_limits<double>::max())
            {
                continue;
            }
            
            // 松弛操作：如果经过u到v的距离更短，就更新
            double newDist = dist[u] + weight;
            if (newDist < dist[e.v])
            {
                dist[e.v] = newDist;
                parent[e.v] = u;
                pq.push({dist[e.v], e.v});
            }
        }
    }

    // ---- 第5步：回溯路径 ----
    QVector<int> path;
    
    // 如果终点不可达，返回空路径
    if (dist[endId] == std::numeric_limits<double>::max())
    {
        return path;
    }
    
    // 从终点往回走，构建路径
    int curr = endId;
    while (curr != startId)
    {
        path.append(curr);
        curr = parent[curr];
    }
    path.append(startId);
    
    // 反转路径（因为我们是从终点往起点走的）
    std::reverse(path.begin(), path.end());
    
    return path;
}

// ============================================================
// 校车相关逻辑
// ============================================================

// ============================================================
// 获取下一班车时间
// 根据当前时刻和天气，返回最近的一班车发车时间
// ============================================================
QTime GraphModel::getNextBusTime(int stationId, QTime arrivalTime, Weather weather) const
{
    // 检查这个站点是否有时刻表
    if (!stationSchedules.contains(stationId))
    {
        return QTime();  // 返回无效时间
    }
    
    // 计算天气导致的延误时间
    int delayMinutes = 0;
    if (weather == Weather::Rainy)
    {
        delayMinutes = 5;   // 下雨延误5分钟
    }
    if (weather == Weather::Snowy)
    {
        delayMinutes = 15;  // 下雪延误15分钟
    }

    // 遍历时刻表，找第一班晚于到达时间的车
    const QVector<QTime>& rawTimes = stationSchedules[stationId];
    for (const QTime& rawT : rawTimes)
    {
        // 加上延误时间得到实际发车时间
        QTime effectiveT = rawT.addSecs(delayMinutes * 60);
        
        // 如果这班车在我们到达之后发车，就坐这班
        if (effectiveT >= arrivalTime)
        {
            return effectiveT;
        }
    }
    
    return QTime();  // 没有合适的班次
}

// ============================================================
// 计算最佳校车路线
// 会尝试所有可能的上车站和下车站组合，找最快的
// ============================================================
GraphModel::BusRouteResult GraphModel::calculateBestBusRoute(
    int startId,
    int endId,
    QTime currentTime,
    Weather weather)
{
    BusRouteResult bestResult;
    bestResult.valid = false;
    bestResult.totalDuration = std::numeric_limits<double>::max();

    // 找出所有公交站
    QVector<int> stations;
    for (auto it = nodesMap.begin(); it != nodesMap.end(); ++it)
    {
        if (it.value().category == NodeCategory::BusStation)
        {
            stations.append(it.key());
        }
    }
    
    if (stations.isEmpty())
    {
        return bestResult;
    }

    // 遍历所有上车站
    for (int startStation : stations)
    {
        // 第1段：步行到上车站
        QVector<int> walk1Path = findPath(startId, startStation, TransportMode::Walk, weather);
        if (walk1Path.isEmpty())
        {
            continue;
        }
        
        double walk1Time = calculateDuration(walk1Path, TransportMode::Walk, weather);
        QTime arrivalAtStation = currentTime.addSecs((int)walk1Time);
        
        // 查询下一班车
        QTime busTime = getNextBusTime(startStation, arrivalAtStation, weather);
        if (!busTime.isValid())
        {
            continue;
        }

        double waitTime = arrivalAtStation.secsTo(busTime);

        // 遍历所有下车站
        for (int endStation : stations)
        {
            if (startStation == endStation)
            {
                continue;
            }
            
            // 第2段：坐校车
            QVector<int> ridePath = findPath(startStation, endStation, TransportMode::Bus, weather);
            if (ridePath.isEmpty())
            {
                continue;
            }
            
            double rideTime = calculateDuration(ridePath, TransportMode::Bus, weather);
            
            // 第3段：从下车站步行到终点
            QVector<int> walk2Path = findPath(endStation, endId, TransportMode::Walk, weather);
            if (walk2Path.isEmpty())
            {
                continue;
            }
            
            double walk2Time = calculateDuration(walk2Path, TransportMode::Walk, weather);

            // 计算总时间
            double total = walk1Time + waitTime + rideTime + walk2Time;
            
            // 如果比当前最优解更好，更新
            if (total < bestResult.totalDuration)
            {
                bestResult.valid = true;
                bestResult.totalDuration = total;
                bestResult.stationStartId = startStation;
                bestResult.stationEndId = endStation;
                bestResult.nextBusTime = busTime;
                
                // 拼接完整路径
                bestResult.fullPath = walk1Path;
                if (!ridePath.isEmpty())
                {
                    for (int k = 1; k < ridePath.size(); ++k)
                    {
                        bestResult.fullPath.append(ridePath[k]);
                    }
                }
                if (!walk2Path.isEmpty())
                {
                    for (int k = 1; k < walk2Path.size(); ++k)
                    {
                        bestResult.fullPath.append(walk2Path[k]);
                    }
                }
            }
        }
    }
    
    return bestResult;
}

// ============================================================
// 判断是否会迟到
// ============================================================
bool GraphModel::isLate(double durationSeconds, QTime current, QTime target) const
{
    QTime arrival = current.addSecs((int)durationSeconds);
    return arrival > target;
}

// ============================================================
// 多段路径拼接（支持途经点）
// ============================================================
QVector<int> GraphModel::findMultiStagePath(
    int startId,
    int endId,
    const QVector<int>& waypoints,
    TransportMode mode,
    Weather weather,
    WeightMode weightMode)
{
    QVector<int> fullPath;
    int currentStart = startId;
    
    // 构建目标点列表：途经点 + 终点
    QVector<int> targets = waypoints;
    targets.append(endId);

    // 逐段规划路径
    for (int target : targets)
    {
        QVector<int> segment = findPath(currentStart, target, mode, weather, weightMode);
        
        // 如果某一段不可达，整个路径失败
        if (segment.isEmpty())
        {
            return {};
        }
        
        // 避免重复节点：如果不是第一段，去掉起点
        if (!fullPath.isEmpty())
        {
            segment.removeFirst();
        }
        
        fullPath.append(segment);
        currentStart = target;
    }
    
    return fullPath;
}

// ============================================================
// 多策略路径推荐 - 核心函数
// 为用户提供3种不同策略的路线选择
// ============================================================
QVector<PathRecommendation> GraphModel::getMultiStrategyRoutes(
    int startId,
    int endId,
    const QVector<int>& waypoints,
    TransportMode mode,
    Weather weather,
    QTime currentTime,
    QTime classTime,
    bool enableLateCheck)
{
    QVector<PathRecommendation> results;

    // ---- 校车模式：特殊处理 ----
    if (mode == TransportMode::Bus)
    {
        BusRouteResult busRes = calculateBestBusRoute(startId, endId, currentTime, weather);
        
        if (busRes.valid)
        {
            bool late = enableLateCheck && isLate(busRes.totalDuration, currentTime, classTime);
            double dist = calculateDistance(busRes.fullPath);
            
            results.append(PathRecommendation(
                RouteType::FASTEST,
                "校车通勤",
                QString("班次 %1").arg(busRes.nextBusTime.toString("HH:mm")),
                busRes.fullPath,
                dist,
                busRes.totalDuration,
                0,
                late
            ));
        }
        
        return results;
    }

    // ---- 策略A：极限冲刺（最快到达）----
    {
        QVector<int> path = findMultiStagePath(startId, endId, waypoints, mode, weather, WeightMode::TIME);
        
        if (!path.isEmpty())
        {
            double dist = calculateDistance(path);
            double dur = calculateDuration(path, mode, weather);
            bool late = enableLateCheck && isLate(dur, currentTime, classTime);
            
            results.append(PathRecommendation(
                RouteType::FASTEST,
                "极限冲刺",
                "最快到达",
                path,
                dist,
                dur,
                0,
                late
            ));
        }
    }

    // ---- 策略B：懒人养生（避开楼梯和坡道）----
    if (mode != TransportMode::Run) {
        QVector<int> path = findMultiStagePath(startId, endId, waypoints, mode, weather, WeightMode::COST);
        // 简单去重：如果路径和“极限冲刺”不一样才加
        if (!path.isEmpty() && (results.isEmpty() || path != results.last().pathNodeIds)) {
            double dist = calculateDistance(path);
            double dur = calculateDuration(path, mode, weather);
            bool late = enableLateCheck && isLate(dur, currentTime, classTime);
            results.append(PathRecommendation(RouteType::EASIEST, "懒人养生", "平坦舒适", path, dist, dur, 0, late));
        }
    }

    // 策略C: 经济适用 (Distance) - 仅步行
    if (mode == TransportMode::Walk) {
        QVector<int> path = findMultiStagePath(startId, endId, waypoints, mode, weather, WeightMode::DISTANCE);
        // 去重
        bool isUnique = true;
        for(const auto& r : results) if(r.pathNodeIds == path) isUnique = false;
        
        if (!path.isEmpty() && isUnique) {
            double dist = calculateDistance(path);
            double dur = calculateDuration(path, mode, weather);
            bool late = enableLateCheck && isLate(dur, currentTime, classTime);
            results.append(PathRecommendation(RouteType::SHORTEST, "经济适用", "路程最短", path, dist, dur, 0, late));
        }
    }

    return results;
}

// ============================================================
// 计算路径总时长（秒）
// 遍历路径上的每条边，累加时间权重
// ============================================================
double GraphModel::calculateDuration(const QVector<int>& pathNodeIds, TransportMode mode, Weather weather) const
{
    double total = 0;
    
    // 遍历路径上每一条边
    for (int i = 0; i < pathNodeIds.size() - 1; ++i)
    {
        const Edge* edge = findEdge(pathNodeIds[i], pathNodeIds[i + 1]);
        
        if (edge)
        {
            // 使用TIME权重模式计算这条边的通行时间
            total += getEdgeWeight(*edge, WeightMode::TIME, mode, weather);
        }
    }
    
    return total;
}

// ============================================================
// 计算路径总距离（米）
// 遍历路径上的每条边，累加物理距离
// ============================================================
double GraphModel::calculateDistance(const QVector<int>& pathNodeIds) const
{
    double total = 0;
    
    for (int i = 0; i < pathNodeIds.size() - 1; ++i)
    {
        const Edge* edge = findEdge(pathNodeIds[i], pathNodeIds[i + 1]);
        
        if (edge)
        {
            total += edge->distance;
        }
    }
    
    return total;
}

// ============================================================
// 计算路径总成本
// 目前成本等同于距离，未来可扩展（如考虑楼梯数量等）
// ============================================================
double GraphModel::calculateCost(const QVector<int>& pathNodeIds) const
{
    return calculateDistance(pathNodeIds);
}

// ============================================================
// 查找两个节点之间的边
// 支持无向边：u->v 和 v->u 都会被找到
// ============================================================
const Edge* GraphModel::findEdge(int u, int v) const
{
    for (const auto& edge : edgesList)
    {
        // 检查正向或反向
        bool match = (edge.u == u && edge.v == v);
        bool reverseMatch = (edge.u == v && edge.v == u);
        
        if (match || reverseMatch)
        {
            return &edge;
        }
    }
    
    return nullptr;  // 未找到
}

// ============================================================
// 简单的getter函数
// ============================================================
Node GraphModel::getNode(int id)
{
    return nodesMap.value(id);
}

Node* GraphModel::getNodePtr(int id)
{
    if (nodesMap.contains(id))
    {
        return &nodesMap[id];
    }
    return nullptr;
}

QVector<Node> GraphModel::getAllNodes() const
{
    return nodesMap.values();
}

QVector<Edge> GraphModel::getAllEdges() const
{
    return edgesList;
}