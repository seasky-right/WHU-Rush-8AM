#ifndef GRAPHMODEL_H
#define GRAPHMODEL_H

#include "../GraphData.h"
#include <QMap>
#include <QString>
#include <QVector>

class GraphModel {
public:
    GraphModel();

    // 加载数据
    bool loadData(const QString& nodesPath, const QString& edgesPath);

    // 核心寻路
    QVector<int> findPath(int startId, int endId);

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
};

#endif // GRAPHMODEL_H
