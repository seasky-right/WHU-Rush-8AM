#include "RouteButton.h"
// Module-qualified Qt includes
#include <QtCore/QPropertyAnimation>
#include <QtGui/QEnterEvent>
#include <QtCore/QEvent>
#include <QtWidgets/QSizePolicy>


RouteButton::RouteButton(const PathRecommendation& rec, QWidget *parent)
    : QPushButton(parent), recommendation(rec)
{
    // 显示更丰富的信息
    QString text = recommendation.getDisplayText();
    if (recommendation.isLate) {
        text += "\n⚠️ 预计迟到";
    }
    setText(text);
    
    // 初始样式
    updateStyle(false);
    
    // 统一尺寸策略：宽度填满，固定高度
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(65);
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
    QString baseColor = "#FFFFFF";
    QString hoverColor = "#F0F0F2";
    QString borderColor = "transparent";
    QString textColor = "#1C1C1E";

    // 迟到颜色逻辑
    if (recommendation.isLate) {
        baseColor = "#FFF5F5";
        hoverColor = "#FFEBEB";
        borderColor = "#FFE0E0";
        textColor = "#C62828";
    }

    if (isHovered) {
        // 悬停时的样式 - iOS风格卡片
        QString style = QString(
            "RouteButton { "
            "    background-color: %1; "  
            "    color: %2; "              
            "    font-weight: 600; "
            "    font-size: 12px; "
            "    border: none; "   
            "    border-radius: 12px; "
            "    padding: 10px 12px; "
            "    text-align: left; "
            "} "
        ).arg(hoverColor).arg(textColor);
        
        setStyleSheet(style);
    } else {
        // 正常样式 - iOS风格卡片
        QString style = QString(
            "RouteButton { "
            "    background-color: %1; "
            "    color: %2; "
            "    font-weight: 500; "
            "    font-size: 12px; "
            "    border: 1px solid %3; "
            "    border-radius: 12px; "
            "    padding: 10px 12px; "
            "    text-align: left; "
            "} "
        ).arg(baseColor).arg(textColor).arg(borderColor);
        
        setStyleSheet(style);
    }
}