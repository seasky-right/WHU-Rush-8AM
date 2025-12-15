#pragma once

#include <QObject>
#include <QPointF>

class MapEditor : public QObject {
    Q_OBJECT
public:
    explicit MapEditor(QObject* parent = nullptr);

    void handleMapClick(QPointF scenePos, bool isCtrlPressed);
    // 创建节点（可选创建连边），返回生成的节点 id
    int createNode(const QString& name,
                   const QPointF& pos,
                   int type,
                   const QString& desc = QStringLiteral("无"),
                   const QString& category = QStringLiteral("None"),
                   int connectFromId = -1,
                   bool createEdgeFlag = false,
                   const QString& edgeName = QStringLiteral("自动道路"),
                   const QString& edgeDesc = QStringLiteral("无"));
    void resetConnection();

private:
    int m_buildingIdCounter;   // starts at 100
    int m_roadIdCounter;       // starts at 900
    int m_lastConnectedId;     // starts at -1

    void appendNode(int id, const QString& name, const QPointF& pos,
                    int type, const QString& desc = QStringLiteral("无"),
                    const QString& category = QStringLiteral("None"));
    void appendEdge(int u, int v, double distance = 0.0, int type = 0,
                    const QString& isSlope = QStringLiteral("false"),
                    const QString& name = QStringLiteral("自动道路"),
                    const QString& desc = QString());
};
