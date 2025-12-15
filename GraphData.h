#pragma once

#include <QString>
#include <QVector>

struct Node {
    int id;
    QString name;
    double x, y;
    double z;
    int type;
    QString description;
    QString category;

    QString toString() const {
        return QString("Node[%1]: %2 (%3, %4, z=%5)").arg(id).arg(name).arg(x).arg(y).arg(z);
    }
};

struct Edge {
    int u, v;
    double distance;
    int type;
    bool isSlope;
    double slope;  // 坡度（百分比，如 0.08 = 8%）
    QString name;
    QString description;

    QString toString() const {
        return QString("Edge: %1 -> %2").arg(u).arg(v);
    }
};

 
