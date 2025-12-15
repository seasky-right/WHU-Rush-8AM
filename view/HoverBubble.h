#pragma once

// QGraphicsObject is provided by the Qt Widgets module; use the module-qualified include
// Module-qualified Qt includes
#include <QtCore/QPointF>
#include <QtCore/QRectF>
#include <QtCore/QString>
#include <QtGui/QPainter>
#include <QtWidgets/QStyleOptionGraphicsItem>
#include <QtWidgets/QWidget>
#include <QtWidgets/QGraphicsObject>
#include <QtCore/QString>
#include <QtGui/QColor>
#include <QtGui/QFont>

class HoverBubble : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(qreal bubbleScale READ bubbleScale WRITE setBubbleScale NOTIFY bubbleScaleChanged)
public:
    explicit HoverBubble(QGraphicsItem* parent = nullptr);

    void setContent(const QString& name, const QString& desc);
    void setBaseColor(const QColor& c);
    void setIsEdge(bool isEdge) { m_isEdge = isEdge; }
    void setAngle(double angle) { m_angle = angle; update(); }

    // position the bubble so that its logical center is at scenePos
    void setCenterAt(const QPointF& scenePos);
    // for edges: set the line endpoints (scene coordinates); bubble will include the line
    void setEdgeLine(const QPointF& A, const QPointF& B);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    qreal bubbleScale() const;
    void setBubbleScale(qreal s);

signals:
    void bubbleScaleChanged(qreal);

private:
    QString m_name;
    QString m_desc;
    QColor m_color = QColor(200,200,200,64);
    QFont m_nameFont;
    QFont m_descFont;
    QRectF m_rect;
    // when edge line present, endpoints relative to top-left of m_rect
    QPointF m_lineA_rel;
    QPointF m_lineB_rel;
    QRectF m_textRect_rel;
    bool m_hasLine = false;
    bool m_isEdge = false;
    double m_angle = 0.0; // degrees
    double m_padding = 6.0;
    double m_bubbleRadius = 16.0; // used for node bubble
    double m_bubbleScale = 1.0;
    void recalcLayout();
};

 
