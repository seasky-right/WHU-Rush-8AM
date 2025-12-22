#pragma once

#include "../GraphData.h"
#include "PathRecommendation.h"
#include <QMap>
#include <QString>
#include <QVector>
#include <QStack>
#include <QTime>

/**
 * @brief 用于撤销操作的动作记录结构体
 * 
 * 记录了操作类型以及相关联的节点或边的数据，
 * 以便在撤销时恢复状态。
 */
struct HistoryAction
{
    /**
     * @brief 操作类型枚举
     */
    enum Type
    {
        AddNode,    ///< 添加节点
        DeleteNode, ///< 删除节点
        AddEdge,    ///< 添加边
        DeleteEdge, ///< 删除边
        MoveNode    ///< 移动节点
    };

    Type type;      ///< 操作的具体类型
    Node nodeData;  ///< 涉及的节点数据（如果是节点操作）
    Edge edgeData;  ///< 涉及的边数据（如果是边操作）
};

/**
 * @brief 图模型类
 * 
 * 负责管理地图数据（节点和边），处理数据的加载与保存，
 * 提供编辑器的增删改查接口，以及核心的寻路算法。
 */
class GraphModel
{
public:
    /**
     * @brief 构造函数
     * 
     * 初始化图模型，设置默认的 ID 计数器。
     */
    GraphModel();

    // =========================================================
    //  加载与保存
    // =========================================================

    /**
     * @brief 加载地图数据
     * 
     * 从指定的文件路径读取节点和边的数据。
     * 
     * @param nodesPath 节点数据文件的路径
     * @param edgesPath 边数据文件的路径
     * @return bool 如果加载成功返回 true，否则返回 false
     */
    bool loadData(const QString& nodesPath, const QString& edgesPath);

    /**
     * @brief 加载时刻表
     * 
     * 从 CSV 文件加载校车的时刻表数据。
     * 
     * @param csvPath 时刻表 CSV 文件的路径
     * @return bool 如果加载成功返回 true，否则返回 false
     */
    bool loadSchedule(const QString& csvPath);

    /**
     * @brief 保存地图数据
     * 
     * 将当前的节点和边数据写入到指定的文件中。
     * 
     * @param nodesPath 节点数据保存路径
     * @param edgesPath 边数据保存路径
     * @return bool 如果保存成功返回 true，否则返回 false
     */
    bool saveData(const QString& nodesPath, const QString& edgesPath);

    // =========================================================
    //  基础查询
    // =========================================================

    /**
     * @brief 查找边
     * 
     * 根据两个节点的 ID 查找连接它们的边。
     * 
     * @param u 第一个节点的 ID
     * @param v 第二个节点的 ID
     * @return const Edge* 指向边的指针，如果未找到则返回 nullptr
     */
    const Edge* findEdge(int u, int v) const;

    /**
     * @brief 获取节点指针
     * 
     * @param id 节点的 ID
     * @return Node* 指向节点的指针，如果不存在则返回 nullptr
     */
    Node* getNodePtr(int id);

    /**
     * @brief 获取节点对象
     * 
     * @param id 节点的 ID
     * @return Node 节点的副本，如果不存在则返回默认构造的 Node
     */
    Node getNode(int id);

    /**
     * @brief 获取所有节点
     * 
     * @return QVector<Node> 包含所有节点的列表
     */
    QVector<Node> getAllNodes() const;

    /**
     * @brief 获取所有边
     * 
     * @return QVector<Edge> 包含所有边的列表
     */
    QVector<Edge> getAllEdges() const;

    // =========================================================
    //  编辑器 CRUD 接口
    // =========================================================

    /**
     * @brief 添加节点
     * 
     * 在指定坐标创建一个新节点。
     * 
     * @param x 节点的 X 坐标
     * @param y 节点的 Y 坐标
     * @param type 节点类型（可见建筑或隐形路口）
     * @return int 新创建节点的 ID
     */
    int addNode(double x, double y, NodeType type);

    /**
     * @brief 删除节点
     * 
     * 删除指定 ID 的节点以及所有连接到该节点的边。
     * 
     * @param id 要删除的节点 ID
     */
    void deleteNode(int id);

    /**
     * @brief 更新节点信息
     * 
     * @param n 包含更新后数据的节点对象
     */
    void updateNode(const Node& n);

    /**
     * @brief 添加或更新边
     * 
     * 如果边已存在则更新，否则添加新边。
     * 
     * @param edge 要添加或更新的边对象
     */
    void addOrUpdateEdge(const Edge& edge);

    /**
     * @brief 删除边
     * 
     * 删除连接两个指定节点的边。
     * 
     * @param u 第一个节点的 ID
     * @param v 第二个节点的 ID
     */
    void deleteEdge(int u, int v);

    // =========================================================
    //  撤销功能
    // =========================================================

    /**
     * @brief 记录操作以供撤销
     * 
     * @param action 要记录的历史动作
     */
    void pushAction(const HistoryAction& action);

    /**
     * @brief 执行撤销操作
     * 
     * 恢复到上一步的状态。
     */
    void undo();

    /**
     * @brief 检查是否可以撤销
     * 
     * @return bool 如果撤销栈不为空返回 true，否则返回 false
     */
    bool canUndo() const;

    // =========================================================
    //  寻路与计算
    // =========================================================

    /**
     * @brief 寻找路径
     * 
     * 使用 Dijkstra 算法寻找两点之间的最优路径。
     * 
     * @param startId 起点 ID
     * @param endId 终点 ID
     * @param mode 交通方式
     * @param weather 天气状况
     * @param weightMode 权重模式（时间、距离或代价）
     * @return QVector<int> 路径上经过的节点 ID 列表
     */
    QVector<int> findPath(int startId, int endId, TransportMode mode, Weather weather, WeightMode weightMode = WeightMode::TIME);

    /**
     * @brief 获取多策略路线推荐
     * 
     * 根据不同的策略（如最快、最省力、最短距离）计算推荐路线。
     * 
     * @param startId 起点 ID
     * @param endId 终点 ID
     * @param waypoints 途经点 ID 列表
     * @param mode 交通方式
     * @param weather 天气状况
     * @param currentTime 当前时间
     * @param classTime 上课时间
     * @param enableLateCheck 是否启用迟到检查
     * @return QVector<PathRecommendation> 推荐路线列表
     */
    QVector<PathRecommendation> getMultiStrategyRoutes(
        int startId,
        int endId,
        const QVector<int>& waypoints,
        TransportMode mode,
        Weather weather,
        QTime currentTime,
        QTime classTime,
        bool enableLateCheck
    );

private:
    QMap<int, Node> nodesMap;           ///< 存储所有节点的映射，Key 为 ID
    QVector<Edge> edgesList;            ///< 存储所有边的列表
    QMap<int, QVector<Edge>> adj;       ///< 邻接表，用于快速查找连接关系

    int maxBuildingId = 100;            ///< 建筑 ID 计数器
    int maxRoadId = 10000;              ///< 道路 ID 计数器
    QStack<HistoryAction> undoStack;    ///< 撤销操作栈

    /// 时刻表数据：Key=车站ID, Value=排序后的发车时间列表
    QMap<int, QVector<QTime>> stationSchedules;

    /**
     * @brief 校车计算辅助结构体
     */
    struct BusRouteResult
    {
        bool valid = false;             ///< 方案是否有效
        QVector<int> fullPath;          ///< 完整路径点 (步行1 + 乘车 + 步行2)
        double totalDuration = 0;       ///< 总耗时
        double walk1Duration = 0;       ///< 第一段步行耗时
        double waitDuration = 0;        ///< 等车耗时
        double rideDuration = 0;        ///< 乘车耗时
        double walk2Duration = 0;       ///< 第二段步行耗时
        int stationStartId = -1;        ///< 上车站 ID
        int stationEndId = -1;          ///< 下车站 ID
        QTime nextBusTime;              ///< 实际上车的班次时间
    };

    /**
     * @brief 解析节点行数据
     * @param line 文件中的一行文本
     */
    void parseNodeLine(const QString& line);

    /**
     * @brief 解析边行数据
     * @param line 文件中的一行文本
     */
    void parseEdgeLine(const QString& line);

    /**
     * @brief 解析时刻表行数据
     * @param line 文件中的一行文本
     */
    void parseScheduleLine(const QString& line);

    /**
     * @brief 构建邻接表
     * 
     * 根据 edgesList 重新生成 adj 数据。
     */
    void buildAdjacencyList();

    /**
     * @brief 计算边的权重
     * 
     * @param edge 边对象
     * @param weightMode 权重模式
     * @param transportMode 交通方式
     * @param weather 天气
     * @return double 计算出的权重值
     */
    double getEdgeWeight(const Edge& edge, WeightMode weightMode,
                         TransportMode transportMode, Weather weather) const;

    /**
     * @brief 获取实际速度
     * 
     * @param mode 交通方式
     * @param weather 天气
     * @return double 速度值
     */
    double getRealSpeed(TransportMode mode, Weather weather) const;

    /**
     * @brief 判断是否迟到
     * 
     * @param durationSeconds 行程耗时（秒）
     * @param current 当前时间
     * @param target 目标时间
     * @return bool 如果迟到返回 true
     */
    bool isLate(double durationSeconds, QTime current, QTime target) const;

    /**
     * @brief 计算最优校车方案
     * 
     * @param startId 起点 ID
     * @param endId 终点 ID
     * @param currentTime 当前时间
     * @param weather 天气
     * @return BusRouteResult 最优方案结果
     */
    BusRouteResult calculateBestBusRoute(int startId, int endId, QTime currentTime, Weather weather);

    /**
     * @brief 获取下一班车时间
     * 
     * @param stationId 车站 ID
     * @param arrivalTime 到达车站的时间
     * @param weather 天气（可能导致延误）
     * @return QTime 下一班车的时间
     */
    QTime getNextBusTime(int stationId, QTime arrivalTime, Weather weather) const;

    QString m_nodesPath;    ///< 节点文件路径
    QString m_edgesPath;    ///< 边文件路径

    /**
     * @brief 自动保存
     * 
     * 如果路径已设置，则自动保存数据。
     */
    void autoSave()
    {
        if (m_nodesPath.isEmpty() || m_edgesPath.isEmpty())
        {
            return;
        }
        saveData(m_nodesPath, m_edgesPath);
    }

    /**
     * @brief 寻找多阶段路径
     * 
     * 支持途经点的路径查找。
     */
    QVector<int> findMultiStagePath(int startId, int endId, const QVector<int>& waypoints, TransportMode mode, Weather weather, WeightMode weightMode);

    /**
     * @brief 计算路径总耗时
     */
    double calculateDuration(const QVector<int>& pathNodeIds, TransportMode mode, Weather weather) const;

    /**
     * @brief 计算路径总距离
     */
    double calculateDistance(const QVector<int>& pathNodeIds) const;

    /**
     * @brief 计算路径总代价
     */
    double calculateCost(const QVector<int>& pathNodeIds) const;
};
