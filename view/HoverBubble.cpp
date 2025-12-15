#include "HoverBubble.h"
#include <QPainter>
#include <QFontMetricsF>
#include <algorithm>

HoverBubble::HoverBubble(QGraphicsItem* parent)
    : QGraphicsObject(parent)
{
    m_nameFont = QFont();
    int p = m_nameFont.pointSize()>0 ? m_nameFont.pointSize() : 10;
    m_nameFont.setPointSize(std::max(7, p-1));
    m_nameFont.setBold(true);

    m_descFont = m_nameFont;
    m_descFont.setPointSize(std::max(6, m_nameFont.pointSize()-1));

    setFlag(QGraphicsItem::ItemIgnoresTransformations, false);
    setAcceptHoverEvents(false);
    setScale(m_bubbleScale);
}

void HoverBubble::setContent(const QString& name, const QString& desc)
{
    m_name = name;
    m_desc = desc;
    m_hasLine = false;
    recalcLayout();
    update();
}

void HoverBubble::setBaseColor(const QColor& c)
{
    m_color = c;
    recalcLayout();
    update();
}

void HoverBubble::setCenterAt(const QPointF& scenePos)
{
    // pos the item so the bubble CIRCLE center aligns with scenePos (for nodes)
    // Circle center is at (width/2, bubbleRadius) in local coords
    QRectF br = boundingRect();
    if (m_isEdge) {
        // for edges, center the whole bounding box
        setPos(scenePos - QPointF(br.width()/2.0, br.height()/2.0));
    } else {
        // for nodes, align circle center (at y = m_bubbleRadius) with scenePos
        setPos(scenePos - QPointF(br.width()/2.0, m_bubbleRadius));
    }
}

void HoverBubble::setEdgeLine(const QPointF &A, const QPointF &B)
{
    // compute text sizes
    QFontMetricsF nm(m_nameFont);
    QFontMetricsF dm(m_descFont);
    QSizeF nameSz = nm.size(Qt::TextSingleLine, m_name);
    QSizeF descSz = m_desc.isEmpty() ? QSizeF(0,0) : dm.size(Qt::TextSingleLine, m_desc);

    // midpoint
    QPointF M((A.x()+B.x())/2.0, (A.y()+B.y())/2.0);

    // text box centered at M
    double tw = std::max(nameSz.width(), descSz.width()) + m_padding*2;
    double th = nameSz.height() + (m_desc.isEmpty() ? 0 : (4 + descSz.height())) + m_padding*2;
    QRectF textRect(M.x() - tw/2.0, M.y() - th/2.0, tw, th);

    // line rect
    double minx = std::min(A.x(), B.x());
    double miny = std::min(A.y(), B.y());
    double w = std::abs(A.x()-B.x());
    double h = std::abs(A.y()-B.y());
    QRectF lineRect(minx, miny, w, h);

    QRectF unionRect = lineRect.united(textRect).adjusted(-m_padding, -m_padding, m_padding, m_padding);

    // set relative positions
    m_lineA_rel = A - unionRect.topLeft();
    m_lineB_rel = B - unionRect.topLeft();
    m_textRect_rel = QRectF(textRect.topLeft() - unionRect.topLeft(), textRect.size());

    m_rect = QRectF(0,0, unionRect.width(), unionRect.height());
    m_hasLine = true;

    // set the item position to unionRect.topLeft()
    setPos(unionRect.topLeft());
    update();
}

QRectF HoverBubble::boundingRect() const
{
    return m_rect;
}

void HoverBubble::recalcLayout()
{
    QFontMetricsF nm(m_nameFont);
    QFontMetricsF dm(m_descFont);

    QSizeF nameSz = nm.size(Qt::TextSingleLine, m_name);
    QSizeF descSz = m_desc.isEmpty() ? QSizeF(0,0) : dm.size(Qt::TextSingleLine, m_desc);

    if (m_isEdge) {
        // For edge bubble, use horizontal box around name (desc placed slightly offset)
        double w = std::max(nameSz.width(), descSz.width()) + m_padding*2;
        double h = nameSz.height() + (m_desc.isEmpty() ? 0 : (4 + descSz.height())) + m_padding*2;
        m_rect = QRectF(0, 0, w, h);
    } else {
        // Node bubble: circle big enough to enclose name vertically
        double diam = m_bubbleRadius*2.0;
        double textW = std::max(nameSz.width(), descSz.width());
        double totalW = std::max(diam, textW + m_padding*2);
        // 计算文字总高度：name + 间隔 + desc（如果有）
        double textH = nameSz.height();
        if (!m_desc.isEmpty()) {
            textH += 4 + descSz.height();
        }
        double totalH = diam + textH + m_padding;
        m_rect = QRectF(0, 0, totalW, totalH);
    }
    // ensure scaling origin is center of rect
    setTransformOriginPoint(m_rect.center());
}

qreal HoverBubble::bubbleScale() const
{
    return m_bubbleScale;
}

void HoverBubble::setBubbleScale(qreal s)
{
    if (qFuzzyCompare(m_bubbleScale, s)) return;
    m_bubbleScale = s;
    setScale(s);
    emit bubbleScaleChanged(s);
    update();
}

void HoverBubble::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->save();

    if (m_hasLine) {
        // draw line first (behind bubble)
        QPen pen(m_color);
        // use a moderately thick inflated line, not too heavy
        pen.setWidth(8);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        QColor lineColor = m_color;
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(m_lineA_rel, m_lineB_rel);
    }

    if (m_isEdge) {
        // draw rounded rect background only at the text box (m_textRect_rel), rotated by m_angle
        QRectF tr = m_textRect_rel;
        // draw background rectangle centered at tr
        painter->save();
        QPointF center = tr.center();
        painter->translate(center);
        painter->rotate(m_angle);
        QRectF localRect(-tr.width()/2.0, -tr.height()/2.0, tr.width(), tr.height());
        painter->setBrush(m_color);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(localRect, 6, 6);

        // draw name and desc inside localRect
        painter->setPen(QColor(20,20,20));
        painter->setFont(m_nameFont);
        QFontMetricsF nmf(m_nameFont);
        double nameH = nmf.height();
        QRectF nameRect(localRect.left(), localRect.top() + 4, localRect.width(), nameH);
        painter->drawText(nameRect, Qt::AlignHCenter | Qt::AlignTop, m_name);

        if (!m_desc.isEmpty()) {
            painter->setFont(m_descFont);
            QFontMetricsF dmf(m_descFont);
            double descH = dmf.height();
            QRectF descRect(localRect.left(), nameRect.top() + nameH + 2, localRect.width(), descH);
            painter->drawText(descRect, Qt::AlignHCenter | Qt::AlignTop, m_desc);
        }

        painter->restore();

    } else {
        // node bubble: draw circle then text below/centered
        // 圆心位于上半部分，圆心Y坐标为 bubbleRadius（距顶部）
        QPointF center(m_rect.width()/2.0, m_bubbleRadius);
        painter->setBrush(m_color);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(center, m_bubbleRadius, m_bubbleRadius);

        // draw texts below the circle
        double textStartY = m_bubbleRadius * 2.0 + m_padding/2.0;
        painter->setPen(QColor(20,20,20));
        painter->setFont(m_nameFont);
        
        QFontMetricsF nmf(m_nameFont);
        double nameH = nmf.height();
        QRectF nameRect(0, textStartY, m_rect.width(), nameH);
        painter->drawText(nameRect, Qt::AlignHCenter | Qt::AlignTop, m_name);
        
        if (!m_desc.isEmpty()) {
            painter->setFont(m_descFont);
            QFontMetricsF dmf(m_descFont);
            double descH = dmf.height();
            QRectF descRect(0, textStartY + nameH + 2, m_rect.width(), descH);
            painter->drawText(descRect, Qt::AlignHCenter | Qt::AlignTop, m_desc);
        }
    }

    painter->restore();
}
