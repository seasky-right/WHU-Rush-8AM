#pragma once

#include <QObject>
#include <QPointF>

/**
 * @brief 地图编辑器辅助类
 * 
 * 用于处理地图编辑操作，如点击地图创建节点、自动连接边等。
 * 生成的数据会追加到草稿文件中。
 */
class MapEditor : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * 
     * @param parent 父对象
     */
    explicit MapEditor(QObject* parent = nullptr);

    /**
     * @brief 处理地图点击事件
     * 
     * 根据点击位置和按键状态创建节点。
     * 
     * @param scenePos 点击的场景坐标
     * @param isCtrlPressed 是否按下了 Ctrl 键（决定节点类型）
     */
    void handleMapClick(QPointF scenePos, bool isCtrlPressed);

    /**
     * @brief 创建节点
     * 
     * 创建一个新节点，并可选地与上一个节点创建连接边。
     * 
     * @param name 节点名称
     * @param pos 节点位置
     * @param type 节点类型 (0=可见, 9=隐形)
     * @param desc 描述
     * @param category 类别
     * @param connectFromId 连接源节点 ID (-1 表示不连接)
     * @param createEdgeFlag 是否创建边
     * @param edgeName 边名称
     * @param edgeDesc 边描述
     * @return int 新生成的节点 ID
     */
    int createNode(const QString& name,
                   const QPointF& pos,
                   int type,
                   const QString& desc = QStringLiteral("无"),
                   const QString& category = QStringLiteral("None"),
                   int connectFromId = -1,
                   bool createEdgeFlag = false,
                   const QString& edgeName = QStringLiteral("自动道路"),
                   const QString& edgeDesc = QStringLiteral("无"));

    /**
     * @brief 重置连接状态
     * 
     * 清除上一个连接节点的记录，开始新的路径段。
     */
    void resetConnection();

private:
    int m_buildingIdCounter;   ///< 建筑 ID 计数器 (从 100 开始)
    int m_roadIdCounter;       ///< 道路 ID 计数器 (从 10000 开始)
    int m_lastConnectedId;     ///< 上一个连接的节点 ID (用于连续画路)

    /**
     * @brief 追加节点数据到文件
     */
    void appendNode(int id, const QString& name, const QPointF& pos,
                    int type, const QString& desc = QStringLiteral("无"),
                    const QString& category = QStringLiteral("None"));

    /**
     * @brief 追加边数据到文件
     */
    void appendEdge(int u, int v, double distance = 0.0, int type = 0,
                    const QString& isSlope = QStringLiteral("false"),
                    const QString& name = QStringLiteral("自动道路"),
                    const QString& desc = QString());
};
