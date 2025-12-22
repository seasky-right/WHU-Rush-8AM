#include "MapEditor.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QInputDialog>
#include <QDateTime>
#include <QDir>
#include <QStringConverter>

/**
 * @brief 构造函数
 * 
 * 初始化 ID 计数器和连接状态。
 */
MapEditor::MapEditor(QObject* parent)
    : QObject(parent),
      m_buildingIdCounter(100),
      m_roadIdCounter(10000),
      m_lastConnectedId(-1)
{
}

/**
 * @brief 处理地图点击
 * 
 * @param scenePos 点击位置
 * @param isCtrlPressed Ctrl 键状态
 */
void MapEditor::handleMapClick(QPointF scenePos, bool isCtrlPressed)
{
    int connectFrom = m_lastConnectedId;
    // 如果按下 Ctrl，创建可见建筑(0)，否则创建隐形路口(9)
    createNode(QString(), scenePos, isCtrlPressed ? 0 : 9,
               QStringLiteral("无"), QStringLiteral("None"),
               connectFrom, connectFrom != -1, QStringLiteral("自动道路"), QStringLiteral("无"));
}

/**
 * @brief 重置连接
 */
void MapEditor::resetConnection()
{
    m_lastConnectedId = -1;
}

/**
 * @brief 创建节点
 * 
 * @return int 新节点 ID
 */
int MapEditor::createNode(const QString& name,
                   const QPointF& pos,
                   int type, // 0=Visible, 9=Ghost
                   const QString& desc,
                   const QString& category,
                   int connectFromId,
                   bool createEdgeFlag,
                   const QString& edgeName,
                   const QString& edgeDesc)
{
    int id = -1;
    QString finalName = name;
    
    // 根据类型分配 ID
    if (type == 9)
    {
        id = m_roadIdCounter++;
        if (finalName.trimmed().isEmpty())
        {
            finalName = QStringLiteral("road_%1").arg(id);
        }
    }
    else
    {
        id = m_buildingIdCounter++;
        if (finalName.trimmed().isEmpty())
        {
            finalName = QStringLiteral("building_%1").arg(id);
        }
    }

    // 写入节点数据
    appendNode(id, finalName, pos, type, desc.isEmpty() ? QStringLiteral("无") : desc,
               category.isEmpty() ? QStringLiteral("None") : category);

    // 如果需要，创建连接边
    if (createEdgeFlag && connectFromId != -1)
    {
        appendEdge(connectFromId, id, 0.0, 0, QStringLiteral("false"), edgeName, edgeDesc);
    }

    // 更新上一个节点 ID，以便下次连接
    m_lastConnectedId = id;
    return id;
}

/**
 * @brief 追加节点到文件
 */
void MapEditor::appendNode(int id, const QString& name, const QPointF& pos,
                           int type, const QString& desc, const QString& category)
{
    // 格式: id, name, x, y, z, type, description, category
    // 默认高度 z=30
    const double z = 30.0;

    QFile file(QStringLiteral("nodes_draft.txt"));
    if (!file.open(QIODevice::Append | QIODevice::Text))
    {
        qWarning() << "Failed to open nodes_draft.txt for append";
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << id << ", "
        << name << ", "
        << pos.x() << ", "
        << pos.y() << ", "
        << z << ", "
        << type << ", " // 写入类型整数
        << desc << ", "
        << category << "\n"; 

    file.close();
}

/**
 * @brief 追加边到文件
 */
void MapEditor::appendEdge(int u, int v, double distance, int type,
                           const QString& isSlope, const QString& name,
                           const QString& desc)
{
    QFile file(QStringLiteral("edges_draft.txt"));
    if (!file.open(QIODevice::Append | QIODevice::Text))
    {
        qWarning() << "Failed to open edges_draft.txt for append";
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << u << ", "
        << v << ", "
        << distance << ", "
        << type << ", "
        << isSlope << ", "
        << name << ", "
        << (desc.isEmpty() ? QStringLiteral("无") : desc) << "\n";

    file.close();
}
