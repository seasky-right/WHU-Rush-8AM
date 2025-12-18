#include "MapEditor.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QInputDialog>
#include <QDateTime>
#include <QDir>
#include <QStringConverter>

MapEditor::MapEditor(QObject* parent)
    : QObject(parent),
      m_buildingIdCounter(100),
      m_roadIdCounter(900),
      m_lastConnectedId(-1) {
}

void MapEditor::handleMapClick(QPointF scenePos, bool isCtrlPressed) {
    int connectFrom = m_lastConnectedId;
    createNode(QString(), scenePos, isCtrlPressed ? 0 : 9,
               QStringLiteral("无"), QStringLiteral("None"),
               connectFrom, connectFrom != -1, QStringLiteral("自动道路"), QStringLiteral("无"));
}

void MapEditor::resetConnection() {
    m_lastConnectedId = -1;
}

int MapEditor::createNode(const QString& name,
                   const QPointF& pos,
                   int type, // 0=Visible, 9=Ghost (这里仍接收 int，内部转 enum)
                   const QString& desc,
                   const QString& category,
                   int connectFromId,
                   bool createEdgeFlag,
                   const QString& edgeName,
                   const QString& edgeDesc)
{
    int id = -1;
    QString finalName = name;
    
    // 逻辑判断使用 int 比较
    if (type == 9) {
        id = m_roadIdCounter++;
        if (finalName.trimmed().isEmpty()) {
            finalName = QStringLiteral("road_%1").arg(id);
        }
    } else {
        id = m_buildingIdCounter++;
        if (finalName.trimmed().isEmpty()) {
            finalName = QStringLiteral("building_%1").arg(id);
        }
    }

    appendNode(id, finalName, pos, type, desc.isEmpty() ? QStringLiteral("无") : desc,
               category.isEmpty() ? QStringLiteral("None") : category);

    if (createEdgeFlag && connectFromId != -1) {
        appendEdge(connectFromId, id, 0.0, 0, QStringLiteral("false"), edgeName, edgeDesc);
    }

    m_lastConnectedId = id;
    return id;
}

void MapEditor::appendNode(int id, const QString& name, const QPointF& pos,
                           int type, const QString& desc, const QString& category) {
    // Format: id, name, x, y, z, type, description, category
    // Defaults: z=30
    const double z = 30.0;

    QFile file(QStringLiteral("nodes_draft.txt"));
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
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
        << type << ", " // 这里写入 int (0 或 9)，符合 GraphModel 解析预期
        << desc << ", "
        << category << "\n"; // 写入字符串 (如 "Dorm")

    file.close();
}

void MapEditor::appendEdge(int u, int v, double distance, int type,
                           const QString& isSlope, const QString& name,
                           const QString& desc) {
    QFile file(QStringLiteral("edges_draft.txt"));
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
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