#pragma once

#include <QString>
#include <QVector>
#include <QMap>

// 1. 节点布局类型 (对应原 int type: 0=Visible, 9=Ghost)
enum class NodeType {
    Visible = 0,
    Ghost = 9
};

// 2. 节点功能分类 (对应原 string category)
// 包括 nodes.txt 中出现的所有类型
enum class NodeCategory {
    None,
    Dorm,       // 宿舍
    Canteen,    // 食堂
    Service,    // 服务设施(快递等)
    Square,     // 广场
    Gate,       // 校门
    Road,       // 道路点
    Park,       // 公园/绿地
    Shop,       // 超市/商店
    Playground, // 操场
    Landmark,   // 地标
    Lake,       // 湖泊
    Building,   // 一般建筑
    Classroom,  // 教学楼
    Hotel       // 宾馆/招待所
};

// 3. 边类型 (对应原 int type)
enum class EdgeType {
    Normal = 0, // 普通道路
    Main = 1,   // 主干道
    Path = 2,   // 小径/绿道
    Indoor = 3  // 楼内连线
};

struct Node {
    int id;
    QString name;
    double x, y;
    double z;
    NodeType type;          // 已更新为 Enum
    QString description;
    NodeCategory category;  // 已更新为 Enum

    QString toString() const {
        return QString("Node[%1]: %2 (%3, %4, z=%5)").arg(id).arg(name).arg(x).arg(y).arg(z);
    }
    
    // 辅助：获取分类的字符串表示（用于UI显示或保存）
    static QString categoryToString(NodeCategory c) {
        switch(c) {
            case NodeCategory::Dorm: return "Dorm";
            case NodeCategory::Canteen: return "Canteen";
            case NodeCategory::Service: return "Service";
            case NodeCategory::Square: return "Square";
            case NodeCategory::Gate: return "Gate";
            case NodeCategory::Road: return "Road";
            case NodeCategory::Park: return "Park";
            case NodeCategory::Shop: return "Shop";
            case NodeCategory::Playground: return "Playground";
            case NodeCategory::Landmark: return "Landmark";
            case NodeCategory::Lake: return "Lake";
            case NodeCategory::Building: return "Building";
            case NodeCategory::Classroom: return "Classroom";
            case NodeCategory::Hotel: return "Hotel";
            default: return "None";
        }
    }

    static NodeCategory stringToCategory(const QString& s) {
        static QMap<QString, NodeCategory> map = {
            {"Dorm", NodeCategory::Dorm}, {"Canteen", NodeCategory::Canteen},
            {"Service", NodeCategory::Service}, {"Square", NodeCategory::Square},
            {"Gate", NodeCategory::Gate}, {"Road", NodeCategory::Road},
            {"Park", NodeCategory::Park}, {"Shop", NodeCategory::Shop},
            {"Playground", NodeCategory::Playground}, {"Landmark", NodeCategory::Landmark},
            {"Lake", NodeCategory::Lake}, {"Building", NodeCategory::Building},
            {"Classroom", NodeCategory::Classroom}, {"Hotel", NodeCategory::Hotel},
            {"None", NodeCategory::None}
        };
        return map.value(s, NodeCategory::None);
    }
};

struct Edge {
    int u, v;
    double distance;
    EdgeType type; // 已更新为 Enum
    bool isSlope;
    double slope;
    QString name;
    QString description;

    QString toString() const {
        return QString("Edge: %1 -> %2").arg(u).arg(v);
    }
};