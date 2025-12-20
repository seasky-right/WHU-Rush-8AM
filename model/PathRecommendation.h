#pragma once

#include <QVector>
#include <QString>

enum class RouteType {
    FASTEST,      // 极限冲刺
    EASIEST,      // 懒人养生
    SHORTEST      // 经济适用
};

struct PathRecommendation {
    RouteType type;
    QString typeName;           // "极限冲刺" / "懒人养生" / "经济适用"
    QString routeLabel;         // "路线1" / "路线2" / "路线3"
    QVector<int> pathNodeIds;   // 路径经过的节点ID列表
    double distance;            // 总距离（米）
    double duration;            // 总耗时（秒）
    double cost;                // 心理代价权重值
    bool isLate;                // [新增] 是否会迟到

    PathRecommendation()
        : type(RouteType::FASTEST), distance(0), duration(0), cost(0), isLate(false) {}

    // [修改] 构造函数包含了 isLate
    PathRecommendation(RouteType t, QString name, QString label, 
                      const QVector<int>& path, double dist, double dur, double c, bool late = false)
        : type(t), typeName(name), routeLabel(label), 
          pathNodeIds(path), distance(dist), duration(dur), cost(c), isLate(late) {}

    QString getDisplayText() const {
        return QString("%1 | %2 | 距离: %3m | 耗时: %4s")
            .arg(routeLabel)
            .arg(typeName)
            .arg(static_cast<int>(distance))
            .arg(static_cast<int>(duration));
    }
};