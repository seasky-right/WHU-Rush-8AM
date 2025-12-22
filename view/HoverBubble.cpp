// ============================================================
// HoverBubble.cpp - 悬停信息气泡
// 当鼠标悬停在地图上的节点或道路时，显示一个信息气泡
// ============================================================

#include "HoverBubble.h"
#include <QtGui/QPainter>
#include <QtGui/QFontMetricsF>
#include <algorithm>

// ============================================================
// 构造函数 - 初始化气泡
// ============================================================
HoverBubble::HoverBubble(QGraphicsItem* parent)
    : QGraphicsObject(parent)
{
    // ---- 设置字体 ----
    // 优先使用苹果字体，没有则用微软雅黑
    m_nameFont = QFont("PingFang SC", 10);
    if (!QFontInfo(m_nameFont).exactMatch())
    {
        m_nameFont.setFamily("Microsoft YaHei");
    }
    m_nameFont.setWeight(QFont::Bold);

    // 描述文字用普通粗细，稍小一点
    m_descFont = m_nameFont;
    m_descFont.setWeight(QFont::Normal);
    m_descFont.setPointSize(9);

    // ---- 关键设置：忽略缩放变换 ----
    // 这样地图缩小时，气泡大小不变，始终保持清晰可读
    setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    
    setAcceptHoverEvents(false);
    setScale(m_bubbleScale);
}

// ============================================================
// 设置气泡内容
// ============================================================
void HoverBubble::setContent(const QString& name, const QString& desc)
{
    m_name = name;
    m_desc = desc;
    m_hasLine = false;
    
    // 重新计算布局
    recalcLayout();
    update();
}

// ============================================================
// 设置气泡底色
// ============================================================
void HoverBubble::setBaseColor(const QColor& c)
{
    m_color = c;
    recalcLayout();
    update();
}

// ============================================================
// 设置气泡中心位置（用于节点）
// ============================================================
void HoverBubble::setCenterAt(const QPointF& scenePos)
{
    setPos(scenePos);
}

// ============================================================
// 设置边的连线（用于道路）
// 气泡会显示在道路中点位置
// ============================================================
void HoverBubble::setEdgeLine(const QPointF& A, const QPointF& B)
{
    m_hasLine = true;
    
    // 计算道路中点作为气泡位置
    double centerX = (A.x() + B.x()) / 2.0;
    double centerY = (A.y() + B.y()) / 2.0;
    QPointF center(centerX, centerY);
    setPos(center);
    
    // 记录连线的相对坐标，用于绘制
    m_lineA_rel = mapFromScene(A);
    m_lineB_rel = mapFromScene(B);
    
    recalcLayout();
    update();
}

// ============================================================
// 重新计算气泡的尺寸和位置
// ============================================================
void HoverBubble::recalcLayout()
{
    // 根据文字内容计算需要的宽高
    QFontMetricsF nmf(m_nameFont);
    QFontMetricsF dmf(m_descFont);

    double w = nmf.horizontalAdvance(m_name);
    double h = nmf.height();

    // 如果有描述文字，增加高度
    if (!m_desc.isEmpty())
    {
        double descWidth = dmf.horizontalAdvance(m_desc);
        if (descWidth > w)
        {
            w = descWidth;
        }
        h = h + dmf.height() + 2;
    }

    // 加上内边距
    w = w + m_padding * 2;
    h = h + m_padding;

    // 保证最小宽度
    double minW = 80.0;
    if (w < minW)
    {
        w = minW;
    }

    // 根据是边还是节点，设置不同的位置
    if (m_isEdge)
    {
        // 边的气泡：居中显示
        m_rect = QRectF(-w / 2.0, -h / 2.0, w, h);
    }
    else
    {
        // 节点的气泡：显示在节点下方
        double nodeRadius = 10.0;
        double topMargin = 8.0;
        m_rect = QRectF(-w / 2.0, nodeRadius + topMargin, w, h);
    }
}

// ============================================================
// 返回气泡的边界矩形（Qt绘图系统需要）
// ============================================================
QRectF HoverBubble::boundingRect() const
{
    QRectF r = m_rect;
    
    // 如果有连线，扩展边界以包含连线
    if (m_hasLine)
    {
        r = r.united(QRectF(m_lineA_rel, QSizeF(1, 1)));
        r = r.united(QRectF(m_lineB_rel, QSizeF(1, 1)));
    }
    
    // 留一点余量
    return r.adjusted(-10, -10, 10, 10);
}

// ============================================================
// 绘制气泡 - Qt会自动调用这个函数来画气泡
// ============================================================
void HoverBubble::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    // 开启抗锯齿，让线条更平滑
    painter->setRenderHint(QPainter::Antialiasing);

    // ---- 第1步：如果是道路气泡，先画连接线 ----
    if (m_hasLine)
    {
        painter->save();
        
        // 设置线条样式
        QColor lineColor = m_color.darker(110);
        lineColor.setAlpha(100);
        
        QPen linePen(lineColor);
        linePen.setWidth(4);
        linePen.setCapStyle(Qt::RoundCap);
        
        painter->setPen(linePen);
        painter->drawLine(m_lineA_rel, m_lineB_rel);
        
        painter->restore();
    }

    // ---- 第2步：画气泡本体（圆角矩形） ----
    painter->save();
    painter->setBrush(m_color);
    
    // 边框用很淡的灰色
    QPen borderPen(QColor(0, 0, 0, 15));
    borderPen.setWidthF(1.0);
    painter->setPen(borderPen);

    double radius = 12.0;  // 圆角半径
    painter->drawRoundedRect(m_rect, radius, radius);

    // ---- 第3步：画文字 ----
    QColor textColor("#1C1C1E");   // 深灰色
    QColor descColor("#8E8E93");   // 浅灰色

    // 画名称
    painter->setPen(textColor);
    painter->setFont(m_nameFont);
    
    QFontMetricsF nmf(m_nameFont);
    double nameH = nmf.height();
    QRectF nameRect(m_rect.left(), m_rect.top() + m_padding / 2.0, m_rect.width(), nameH);
    painter->drawText(nameRect, Qt::AlignHCenter | Qt::AlignVCenter, m_name);

    // 画描述（如果有的话）
    if (!m_desc.isEmpty())
    {
        painter->setPen(descColor);
        painter->setFont(m_descFont);
        
        QFontMetricsF dmf(m_descFont);
        double descH = dmf.height();
        QRectF descRect(m_rect.left(), nameRect.bottom(), m_rect.width(), descH);
        painter->drawText(descRect, Qt::AlignHCenter | Qt::AlignVCenter, m_desc);
    }

    painter->restore();
}

// ============================================================
// 获取当前缩放比例
// ============================================================
qreal HoverBubble::bubbleScale() const
{
    return m_bubbleScale;
}

// ============================================================
// 设置缩放比例（用于动画效果）
// ============================================================
void HoverBubble::setBubbleScale(qreal s)
{
    if (m_bubbleScale != s)
    {
        m_bubbleScale = s;
        setScale(s);
        emit bubbleScaleChanged(s);
    }
}