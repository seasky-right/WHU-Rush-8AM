// ============================================================
// MapEditor.cpp - 地图编辑器的核心逻辑
// 负责创建新节点、追加数据到文件
// ============================================================

#include "MapEditor.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QInputDialog>
#include <QDateTime>
#include <QDir>
#include <QStringConverter>

// ============================================================
// 构造函数 - 初始化编辑器
// ============================================================
MapEditor::MapEditor(QObject* parent)
    : QObject(parent),
      m_buildingIdCounter(100),      // 建筑ID从100开始
      m_roadIdCounter(10000),        // 路口ID从10000开始
      m_lastConnectedId(-1)          // -1表示还没有上一个连接点
{
}

// ============================================================
// 处理地图点击事件
// 当用户在地图上点击时，这个函数会被调用
// ============================================================
void MapEditor::handleMapClick(QPointF scenePos, bool isCtrlPressed)
{
    // 记住上一个节点的ID，用于自动连线
    int connectFrom = m_lastConnectedId;
    
    // 根据是否按住Ctrl键，决定创建什么类型的节点：
    // - 按住Ctrl：创建可见的建筑物（type=0）
    // - 没按Ctrl：创建隐形的路口（type=9）
    int nodeType = 0;
    if (isCtrlPressed)
    {
        nodeType = 0;  // 可见建筑
    }
    else
    {
        nodeType = 9;  // 隐形路口
    }
    
    // 判断是否需要自动连线
    bool shouldConnect = (connectFrom != -1);
    
    // 调用创建节点的函数
    createNode(
        QString(),                    // 名称（空字符串表示自动生成）
        scenePos,                     // 点击位置
        nodeType,                     // 节点类型
        QStringLiteral("无"),         // 描述
        QStringLiteral("None"),       // 分类
        connectFrom,                  // 连接起点
        shouldConnect,                // 是否创建连线
        QStringLiteral("自动道路"),   // 道路名称
        QStringLiteral("无")          // 道路描述
    );
}

// ============================================================
// 重置连接状态
// 清除"上一个节点"的记录，下次点击不会自动连线
// ============================================================
void MapEditor::resetConnection()
{
    m_lastConnectedId = -1;
}

// ============================================================
// 创建节点 - 核心函数
// 在地图上创建一个新的节点，并可选地与上一个节点连线
// ============================================================
int MapEditor::createNode(
    const QString& name,
    const QPointF& pos,
    int type,
    const QString& desc,
    const QString& category,
    int connectFromId,
    bool createEdgeFlag,
    const QString& edgeName,
    const QString& edgeDesc)
{
    // ---- 第1步：分配ID ----
    int id = -1;
    QString finalName = name;
    
    if (type == 9)
    {
        // 隐形路口：ID从10000开始递增
        id = m_roadIdCounter;
        m_roadIdCounter = m_roadIdCounter + 1;
        
        // 如果没有指定名称，自动生成一个
        if (finalName.trimmed().isEmpty())
        {
            finalName = QStringLiteral("road_%1").arg(id);
        }
    }
    else
    {
        // 可见建筑：ID从100开始递增
        id = m_buildingIdCounter;
        m_buildingIdCounter = m_buildingIdCounter + 1;
        
        if (finalName.trimmed().isEmpty())
        {
            finalName = QStringLiteral("building_%1").arg(id);
        }
    }

    // ---- 第2步：把节点写入文件 ----
    QString description = desc;
    if (description.isEmpty())
    {
        description = QStringLiteral("无");
    }
    
    QString cat = category;
    if (cat.isEmpty())
    {
        cat = QStringLiteral("None");
    }
    
    appendNode(id, finalName, pos, type, description, cat);

    // ---- 第3步：如果需要，创建连接边 ----
    if (createEdgeFlag)
    {
        if (connectFromId != -1)
        {
            appendEdge(connectFromId, id, 0.0, 0, 
                       QStringLiteral("false"), edgeName, edgeDesc);
        }
    }

    // ---- 第4步：记住这个节点，下次自动连线用 ----
    m_lastConnectedId = id;
    
    return id;
}

// ============================================================
// 追加节点到文件
// 把节点信息写入 nodes_draft.txt 文件
// ============================================================
void MapEditor::appendNode(
    int id,
    const QString& name,
    const QPointF& pos,
    int type,
    const QString& desc,
    const QString& category)
{
    // 默认高度设为30米
    const double z = 30.0;

    // 打开文件（追加模式）
    QFile file(QStringLiteral("nodes_draft.txt"));
    bool opened = file.open(QIODevice::Append | QIODevice::Text);
    
    if (!opened)
    {
        qWarning() << "无法打开 nodes_draft.txt 文件";
        return;
    }

    // 把节点信息写入文件
    // 格式: id, name, x, y, z, type, description, category
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    out << id << ", "
        << name << ", "
        << pos.x() << ", "
        << pos.y() << ", "
        << z << ", "
        << type << ", "
        << desc << ", "
        << category << "\n";

    file.close();
}

// ============================================================
// 追加边到文件
// 把道路信息写入 edges_draft.txt 文件
// ============================================================
void MapEditor::appendEdge(
    int u,
    int v,
    double distance,
    int type,
    const QString& isSlope,
    const QString& name,
    const QString& desc)
{
    // 打开文件（追加模式）
    QFile file(QStringLiteral("edges_draft.txt"));
    bool opened = file.open(QIODevice::Append | QIODevice::Text);
    
    if (!opened)
    {
        qWarning() << "无法打开 edges_draft.txt 文件";
        return;
    }

    // 处理空描述
    QString description = desc;
    if (description.isEmpty())
    {
        description = QStringLiteral("无");
    }

    // 把边信息写入文件
    // 格式: u, v, distance, type, isSlope, name, description
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    out << u << ", "
        << v << ", "
        << distance << ", "
        << type << ", "
        << isSlope << ", "
        << name << ", "
        << description << "\n";

    file.close();
}

