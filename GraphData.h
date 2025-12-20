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
    Classroom, Hotel, BusStation
};

// 3. 边类型
enum class EdgeType {
    Normal = 0, Main = 1, Path = 2, Indoor = 3, Stairs = 4
};

// 4. [新增] 天气类型
enum class Weather {
    Sunny,  // 晴天
    Rainy,  // 雨天
    Snowy   // 大雪
};

// 5. [新增] 交通方式 (扩展)
enum class TransportMode {
    Walk,
    SharedBike, // 共享单车
    EBike,      // 电动车
    Run,        // 跑步
    Bus         // 校车
};

// 6. [新增] 权重模式
enum class WeightMode { DISTANCE, TIME, COST };

// 7. [新增] 全局配置常量 (基于 PRD)
namespace Config {
    // 速度 (m/s)
    const double SPEED_WALK = 1.2;          // 步行 4.3 km/h
    const double SPEED_RUN = 3.0;           // 跑步 (估算)
    const double SPEED_SHARED_BIKE = 3.89;  // 单车 14.0 km/h
    const double SPEED_EBIKE = 5.56;        // 电驴 20.0 km/h
    const double SPEED_BUS = 10.0;          // 校车 36.0 km/h
    
    // 交互时间 (秒) - 共享单车无桩模式
    const double TIME_FIND_BIKE = 120.0;    // 找车耗时 (2分钟)
    const double TIME_PARK_BIKE = 60.0;     // 还车耗时 (1分钟)
    
    // 阈值
    const double SLOPE_THRESHOLD = 0.05;    // 坡度阈值 5%
}

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
        if (s == "BusStation") return NodeCategory::BusStation;
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
            case NodeCategory::BusStation: return "BusStation";
            default: return "None";
        }
    }
};

struct Edge {
    int u, v;
    double distance;
    EdgeType type;
    double slope;
    QString name;
    QString description;
};