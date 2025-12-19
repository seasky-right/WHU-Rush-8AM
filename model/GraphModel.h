#pragma once

#include "../GraphData.h"
#include "PathRecommendation.h"
#include <QMap>
#include <QString>
#include <QVector>
#include <QStack> // 用于撤销操作

enum class WeightMode { DISTANCE, TIME, COST };

// 用于撤销操作的简单的动作记录
struct HistoryAction {
    enum Type { AddNode, DeleteNode, AddEdge, DeleteEdge, MoveNode };
    Type type;
    // 数据备份
    Node nodeData;
    Edge edgeData;
};

class GraphModel {
public:
    GraphModel();

    // 加载与保存
    bool loadData(const QString& nodesPath, const QString& edgesPath);
    bool saveData(const QString& nodesPath, const QString& edgesPath);

    // 基础查询
    const Edge* findEdge(int u, int v) const;
    Node* getNodePtr(int id); // 获取可修改的指针
    Node getNode(int id);
    QVector<Node> getAllNodes() const;
    QVector<Edge> getAllEdges() const;

    // --- 编辑器 CRUD 接口 ---
    // 添加节点，返回新ID
    int addNode(double x, double y, NodeType type);
    // 删除节点（会自动删除关联边）
    void deleteNode(int id);
    // 更新节点信息
    void updateNode(const Node& n);
    
    // 添加/更新边 (如果已存在则更新，不存在则添加)
    void addOrUpdateEdge(const Edge& edge);
    // 删除边
    void deleteEdge(int u, int v);
    
    // 撤销功能
    void pushAction(const HistoryAction& action);
    void undo();
    bool canUndo() const;

    // 寻路与计算 (保持不变)
    QVector<int> findPath(int startId, int endId);
    QVector<PathRecommendation> recommendPaths(int startId, int endId);
    double calculateDistance(const QVector<int>& pathNodeIds) const;
    double calculateDuration(const QVector<int>& pathNodeIds) const;
    double calculateCost(const QVector<int>& pathNodeIds) const;
    double getEdgeWeight(const Edge& edge, WeightMode mode) const;

private:
    QMap<int, Node> nodesMap;
    QVector<Edge> edgesList;
    QMap<int, QVector<Edge>> adj; // 邻接表
    
    // ID 计数器
    int maxBuildingId = 100;
    int maxRoadId = 900;
    
    // 撤销栈
    QStack<HistoryAction> undoStack;

    void parseNodeLine(const QString& line);
    void parseEdgeLine(const QString& line);
    void buildAdjacencyList();
    double getEffectiveSpeed(double baseSpeed, const Edge& edge, WeightMode mode) const;
    QVector<int> findPathWithMode(int startId, int endId, WeightMode mode);
};