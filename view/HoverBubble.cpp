#include "HoverBubble.h"
#include <QtGui/QPainter>
#include <QtGui/QFontMetricsF>
#include <algorithm>

HoverBubble::HoverBubble(QGraphicsItem* parent)
    : QGraphicsObject(parent)
{
    // iOS 字体设置
    m_nameFont = QFont("PingFang SC", 10);
    if (!QFontInfo(m_nameFont).exactMatch()) m_nameFont.setFamily("Microsoft YaHei");
    m_nameFont.setWeight(QFont::Bold);

    m_descFont = m_nameFont;
    m_descFont.setWeight(QFont::Normal);
    m_descFont.setPointSize(9);

    // 【核心修改】开启此标志！
    // 这会让气泡忽略父级(地图)的缩放变换，永远保持 1:1 的像素大小渲染
    // 从而实现"地图缩小时气泡不缩小"的效果 (Billboarding)
    setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    
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
    setPos(scenePos);
}

void HoverBubble::setEdgeLine(const QPointF& A, const QPointF& B)
{
    m_hasLine = true;
    QPointF center = (A + B) / 2.0;
    setPos(center); 
    
    // 计算相对坐标用于绘制连线
    m_lineA_rel = mapFromScene(A);
    m_lineB_rel = mapFromScene(B);
    
    recalcLayout();
    update();
}

void HoverBubble::recalcLayout()
{
    QFontMetricsF nmf(m_nameFont);
    QFontMetricsF dmf(m_descFont);

    double w = nmf.horizontalAdvance(m_name);
    double h = nmf.height();

    if (!m_desc.isEmpty()) {
        w = std::max(w, dmf.horizontalAdvance(m_desc));
        h += dmf.height() + 2; 
    }

    w += m_padding * 2;
    h += m_padding; 

    double minW = 80.0;
    if (w < minW) w = minW;

    if (m_isEdge) {
        // 边气泡保持居中
        m_rect = QRectF(-w/2.0, -h/2.0, w, h);
    } else {
        // 节点气泡显示在下方
        double nodeRadius = 10.0; 
        double topMargin = 8.0; 
        m_rect = QRectF(-w/2.0, nodeRadius + topMargin, w, h);
    }
}

QRectF HoverBubble::boundingRect() const
{
    QRectF r = m_rect;
    if (m_hasLine) {
        r = r.united(QRectF(m_lineA_rel, QSizeF(1,1)));
        r = r.united(QRectF(m_lineB_rel, QSizeF(1,1)));
    }
    return r.adjusted(-10, -10, 10, 10);
}

void HoverBubble::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing);

    // 1. 绘制连接线 (仅 Edge 模式)
    if (m_hasLine) {
        painter->save();
        QPen linePen(m_color.darker(110)); 
        linePen.setWidth(4);
        linePen.setCapStyle(Qt::RoundCap);
        QColor lineC = linePen.color();
        lineC.setAlpha(100);
        linePen.setColor(lineC);
        
        painter->setPen(linePen);
        painter->drawLine(m_lineA_rel, m_lineB_rel);
        painter->restore();
    }

    // 2. 绘制气泡本体
    painter->save();
    painter->setBrush(m_color);
    
    QPen borderPen(QColor(0, 0, 0, 15)); 
    borderPen.setWidthF(1.0);
    painter->setPen(borderPen);

    double radius = 12.0; 
    painter->drawRoundedRect(m_rect, radius, radius);

    // 3. 绘制文字
    QColor textColor("#1C1C1E"); 
    QColor descColor("#8E8E93"); 

    painter->setPen(textColor);
    painter->setFont(m_nameFont);
    
    QFontMetricsF nmf(m_nameFont);
    double nameH = nmf.height();
    QRectF nameRect(m_rect.left(), m_rect.top() + m_padding/2.0, m_rect.width(), nameH);
    painter->drawText(nameRect, Qt::AlignHCenter | Qt::AlignVCenter, m_name);

    if (!m_desc.isEmpty()) {
        painter->setPen(descColor);
        painter->setFont(m_descFont);
        QFontMetricsF dmf(m_descFont);
        double descH = dmf.height();
        QRectF descRect(m_rect.left(), nameRect.bottom(), m_rect.width(), descH);
        painter->drawText(descRect, Qt::AlignHCenter | Qt::AlignVCenter, m_desc);
    }

    painter->restore();
}

qreal HoverBubble::bubbleScale() const { return m_bubbleScale; }

void HoverBubble::setBubbleScale(qreal s) {
    if (m_bubbleScale != s) {
        m_bubbleScale = s;
        setScale(s);
        emit bubbleScaleChanged(s);
    }
}