#pragma once

#include <QString>
#include <QVector>
#include <QMap>

// 1. 节点类型
enum class NodeType {
    Visible = 0, // 建筑 (显示)
    Ghost = 9    // 路口 (仅编辑模式显示，导航时隐藏)
};

// 2. 节点功能分类
enum class NodeCategory {
    None, Dorm, Canteen, Service, Square, Gate, Road, 
    Park, Shop, Playground, Landmark, Lake, Building, 
    Classroom, Hotel
};

// 3. 边类型
enum class EdgeType {
    Normal = 0, Main = 1, Path = 2, Indoor = 3
};

struct Node {
    int id;
    QString name;
    double x, y;
    double z;
    NodeType type;          
    QString description;
    NodeCategory category; 

    // 辅助转换：字符串转分类
    static NodeCategory stringToCategory(const QString& s) {
        if (s == "Dorm") return NodeCategory::Dorm;
        if (s == "Canteen") return NodeCategory::Canteen;
        if (s == "Service") return NodeCategory::Service;
        if (s == "Square") return NodeCategory::Square;
        if (s == "Gate") return NodeCategory::Gate;
        if (s == "Road") return NodeCategory::Road;
        if (s == "Park") return NodeCategory::Park;
        if (s == "Shop") return NodeCategory::Shop;
        if (s == "Playground") return NodeCategory::Playground;
        if (s == "Landmark") return NodeCategory::Landmark;
        if (s == "Lake") return NodeCategory::Lake;
        if (s == "Building") return NodeCategory::Building;
        if (s == "Classroom") return NodeCategory::Classroom;
        if (s == "Hotel") return NodeCategory::Hotel;
        return NodeCategory::None;
    }
    
    // 辅助转换：分类转字符串 (用于保存)
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
};

struct Edge {
    int u, v;
    double distance;
    EdgeType type;
    bool isSlope;
    double slope;
    QString name;
    QString description;
};