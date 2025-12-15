#pragma once

#include <QVector>
#include <QString>

enum class RouteType {
    FASTEST,      // 极限冲刺 - Weight = Duration
    EASIEST,      // 懒人养生 - Weight = Cost (anti-slope)
    SHORTEST      // 经济适用 - Weight = Distance
};

struct PathRecommendation {
    RouteType type;
    QString typeName;           // "极限冲刺" / "懒人养生" / "经济适用"
    QString routeLabel;         // "路线1" / "路线2" / "路线3"
    QVector<int> pathNodeIds;   // 路径经过的节点ID列表
    double distance;            // 总距离（米）
    double duration;            // 总耗时（秒）
    double cost;                // 心理代价权重值

    PathRecommendation()
        : type(RouteType::FASTEST), distance(0), duration(0), cost(0) {}

    PathRecommendation(RouteType t, QString name, QString label, 
                      const QVector<int>& path, double dist, double dur, double c)
        : type(t), typeName(name), routeLabel(label), 
          pathNodeIds(path), distance(dist), duration(dur), cost(c) {}

    QString getDisplayText() const {
        // 返回按钮显示的文本
        // 例："路线1 | 极限冲刺 | 距离: 800m"
        return QString("%1 | %2 | 距离: %3m | 耗时: %4s")
            .arg(routeLabel)
            .arg(typeName)
            .arg(static_cast<int>(distance))
            .arg(static_cast<int>(duration));
    }
};

 
