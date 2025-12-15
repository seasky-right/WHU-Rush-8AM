#include "RouteButton.h"
// Module-qualified Qt includes
#include <QtCore/QPropertyAnimation>
#include <QtGui/QEnterEvent>
#include <QtCore/QEvent>
#include <QtWidgets/QSizePolicy>


RouteButton::RouteButton(const PathRecommendation& rec, QWidget *parent)
    : QPushButton(parent), recommendation(rec)
{
    // 设置按钮文本
    setText(recommendation.getDisplayText());
    
    // 初始样式
    updateStyle(false);
    
    // 统一尺寸策略：宽度填满，固定高度
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(50);
}

void RouteButton::enterEvent(QEnterEvent *event)
{
    QPushButton::enterEvent(event);
    updateStyle(true);
    emit routeHovered(recommendation);
}

void RouteButton::leaveEvent(QEvent *event)
{
    QPushButton::leaveEvent(event);
    updateStyle(false);
    emit routeUnhovered();
}

void RouteButton::updateStyle(bool isHovered)
{
    if (isHovered) {
        // 悬停时的样式：突出显示
        setStyleSheet(
            "RouteButton { "
            "    background-color: #F8C3CD; "  // 武大樱花粉
            "    color: #2C3E50; "              // 科技灰
            "    font-weight: bold; "
            "    border: 2px solid #E91E63; "   // 粉红色边框
            "    border-radius: 5px; "
            "    padding: 8px; "
            "} "
            "RouteButton:pressed { "
            "    background-color: #E91E63; "
            "    color: white; "
            "}"
        );
    } else {
        // 正常样式
        setStyleSheet(
            "RouteButton { "
            "    background-color: #F0F0F0; "
            "    color: #2C3E50; "
            "    border: 1px solid #D0D0D0; "
            "    border-radius: 5px; "
            "    padding: 8px; "
            "} "
            "RouteButton:pressed { "
            "    background-color: #F8C3CD; "
            "}"
        );
    }
}
