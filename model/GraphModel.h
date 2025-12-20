#pragma once

#include "../GraphData.h"
#include "PathRecommendation.h"
#include <QMap>
#include <QString>
#include <QVector>
#include <QStack>
#include <QTime>

// 用于撤销操作的动作记录
struct HistoryAction {
    enum Type { AddNode, DeleteNode, AddEdge, DeleteEdge, MoveNode };
    Type type;
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
    Node* getNodePtr(int id); 
    Node getNode(int id);
    QVector<Node> getAllNodes() const;
    QVector<Edge> getAllEdges() const;

    // --- 编辑器 CRUD 接口 ---
    int addNode(double x, double y, NodeType type);
    void deleteNode(int id);
    void updateNode(const Node& n);
    void addOrUpdateEdge(const Edge& edge);
    void deleteEdge(int u, int v);
    
    // 撤销功能
    void pushAction(const HistoryAction& action);
    void undo();
    bool canUndo() const;

    // --- 寻路与计算 ---

    QVector<int> findPath(int startId, int endId, TransportMode mode, Weather weather);
    
    // [修改] 获取指定模式的路线详情（替代原先的批量推荐）
    PathRecommendation getSpecificRoute(int startId, int endId, 
                                      TransportMode mode,
                                      Weather weather, 
                                      QTime currentTime, 
                                      QTime classTime);

    double calculateDistance(const QVector<int>& pathNodeIds) const;
    double calculateDuration(const QVector<int>& pathNodeIds, TransportMode mode, Weather weather) const;
    double calculateCost(const QVector<int>& pathNodeIds) const;

private:
    QMap<int, Node> nodesMap;
    QVector<Edge> edgesList;
    QMap<int, QVector<Edge>> adj; 
    
    int maxBuildingId = 100;
    int maxRoadId = 10000;
    QStack<HistoryAction> undoStack;

    void parseNodeLine(const QString& line);
    void parseEdgeLine(const QString& line);
    void buildAdjacencyList();

    double getEdgeWeight(const Edge& edge, WeightMode weightMode, 
                         TransportMode transportMode, Weather weather) const;
    double getRealSpeed(TransportMode mode, Weather weather) const;
    
    bool isLate(double durationSeconds, QTime current, QTime target) const;

    // 【新增】记住文件路径，用于自动保存
    QString m_nodesPath;
    QString m_edgesPath;

    // 【新增】内部辅助函数：执行自动保存
    void autoSave() {
        if (!m_nodesPath.isEmpty() && !m_edgesPath.isEmpty()) {
            saveData(m_nodesPath, m_edgesPath);
        }
    }

};