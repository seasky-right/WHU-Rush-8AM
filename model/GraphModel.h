#pragma once

#include "../GraphData.h"
#include "PathRecommendation.h"
#include <QMap>
#include <QString>
#include <QVector>

// 权重类型枚举
enum class WeightMode {
    DISTANCE,   // 经济适用：仅计算距离
    TIME,       // 极限冲刺：计算时间（考虑坡度加速/减速）
    COST        // 懒人养生：计算心理代价（反感坡度）
};

class GraphModel {
public:
    GraphModel();

    // 加载数据
    bool loadData(const QString& nodesPath, const QString& edgesPath);

    // 核心寻路（单一权重）
    QVector<int> findPath(int startId, int endId);
    QVector<int> findPathWithMode(int startId, int endId, WeightMode mode);

    // 多策略推荐（返回最多3条不同路线）
    QVector<PathRecommendation> recommendPaths(int startId, int endId);

    // 计算路径的各种指标
    double calculateDistance(const QVector<int>& pathNodeIds) const;
    double calculateDuration(const QVector<int>& pathNodeIds) const;
    double calculateCost(const QVector<int>& pathNodeIds) const;
    
    // 获取边的信息
    const Edge* findEdge(int u, int v) const;
    double getEdgeWeight(const Edge& edge, WeightMode mode) const;

    // Getters
    QVector<Node> getAllNodes() const;
    QVector<Edge> getAllEdges() const;
    Node getNode(int id);

private:
    QMap<int, Node> nodesMap;
    QVector<Edge> edgesList;
    QMap<int, QVector<Edge>> adj; // 邻接表

    void parseNodeLine(const QString& line);
    void parseEdgeLine(const QString& line);
    void buildAdjacencyList();

    // 物理计算函数
    double getEffectiveSpeed(double baseSpeed, const Edge& edge, WeightMode mode) const;
};

 
