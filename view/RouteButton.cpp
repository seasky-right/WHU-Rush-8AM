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
        text += "\n(⚠️ 预计迟到)";
    }
    setText(text);
    
    // 初始样式
    updateStyle(false);
    
    // 统一尺寸策略：宽度填满，固定高度
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(60); // 增加高度以容纳可能的警告文字
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
    QString baseColor = "#F0F0F0";
    QString hoverColor = "#F8C3CD"; // 樱花粉
    QString borderColor = "#D0D0D0";
    QString textColor = "#2C3E50";

    // [新增] 迟到颜色逻辑
    if (recommendation.isLate) {
        baseColor = "#FFEBEE";  // 浅红背景 (System Red Light)
        hoverColor = "#FFCDD2"; // 悬停深红
        borderColor = "#E57373"; // 红色边框
        textColor = "#C62828";   // 红色文字
    }

    if (isHovered) {
        // 悬停时的样式
        QString style = QString(
            "RouteButton { "
            "    background-color: %1; "  
            "    color: %2; "              
            "    font-weight: bold; "
            "    border: 2px solid %3; "   
            "    border-radius: 5px; "
            "    padding: 8px; "
            "} "
        ).arg(hoverColor).arg(textColor).arg(recommendation.isLate ? "#D32F2F" : "#E91E63");
        
        setStyleSheet(style);
    } else {
        // 正常样式
        QString style = QString(
            "RouteButton { "
            "    background-color: %1; "
            "    color: %2; "
            "    border: 1px solid %3; "
            "    border-radius: 5px; "
            "    padding: 8px; "
            "} "
        ).arg(baseColor).arg(textColor).arg(borderColor);
        
        setStyleSheet(style);
    }
}