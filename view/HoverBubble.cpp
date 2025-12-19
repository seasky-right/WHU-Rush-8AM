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
        // 【核心修改】节点气泡改为显示在下方
        // y = nodeRadius (假设约8-10) + padding
        double nodeRadius = 10.0; 
        double topMargin = 8.0; 
        
        // 矩形区域：(-宽/2, 节点底部, 宽, 高)
        m_rect = QRectF(-w/2.0, nodeRadius + topMargin, w, h);
    }
}

QRectF HoverBubble::boundingRect() const
{
    QRectF r = m_rect;
    // 如果有线，包围盒要包含线
    if (m_hasLine) {
        r = r.united(QRectF(m_lineA_rel, QSizeF(1,1)));
        r = r.united(QRectF(m_lineB_rel, QSizeF(1,1)));
    }
    // 留出阴影余量
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
        // 线条颜色稍微加深一点点，保证清晰
        QPen linePen(m_color.darker(110)); 
        linePen.setWidth(4);
        linePen.setCapStyle(Qt::RoundCap);
        // 线条稍微透明一点，不抢戏
        QColor lineC = linePen.color();
        lineC.setAlpha(100);
        linePen.setColor(lineC);
        
        painter->setPen(linePen);
        painter->drawLine(m_lineA_rel, m_lineB_rel);
        painter->restore();
    }

    // 2. 绘制气泡本体 (Glassmorphism 风格)
    painter->save();
    
    // 背景：半透明
    painter->setBrush(m_color);
    
    // 描边：极细的浅灰色，增加精致感
    QPen borderPen(QColor(255, 255, 255, 100)); // 半透明白内描边（可选）或浅灰外描边
    borderPen = QPen(QColor(0, 0, 0, 15)); // 极淡的黑色描边，模拟物理边缘
    borderPen.setWidthF(1.0);
    painter->setPen(borderPen);

    double radius = 12.0; // iOS 圆角较大
    painter->drawRoundedRect(m_rect, radius, radius);

    // 3. 绘制文字
    // 必须用深色，保证在半透明背景上的对比度
    QColor textColor("#1C1C1E"); // System Gray (Near Black)
    QColor descColor("#8E8E93"); // System Gray (Secondary)

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