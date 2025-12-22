// ============================================================
// RouteButton.cpp - 路线推荐按钮
// 每一条推荐路线都会显示成一个按钮，点击可以查看详情
// ============================================================

#include "RouteButton.h"
#include <QtCore/QPropertyAnimation>
#include <QtGui/QEnterEvent>
#include <QtCore/QEvent>
#include <QtWidgets/QSizePolicy>

// ============================================================
// 构造函数 - 创建一个路线按钮
// ============================================================
RouteButton::RouteButton(const PathRecommendation& rec, QWidget *parent)
    : QPushButton(parent), recommendation(rec)
{
    // ---- 设置按钮上显示的文字 ----
    QString text = recommendation.getDisplayText();
    
    // 如果这条路线会迟到，加上警告提示
    if (recommendation.isLate)
    {
        text = text + "\n⚠️ 预计迟到";
    }
    
    setText(text);
    
    // ---- 设置初始样式 ----
    updateStyle(false);  // false 表示非悬停状态
    
    // ---- 设置尺寸策略 ----
    // 宽度自动填满父容器，高度固定
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(65);
}

// ============================================================
// 鼠标进入按钮时触发
// ============================================================
void RouteButton::enterEvent(QEnterEvent *event)
{
    // 先调用父类的处理
    QPushButton::enterEvent(event);
    
    // 更新为悬停样式
    updateStyle(true);
    
    // 发出信号，通知外部"用户正在查看这条路线"
    emit routeHovered(recommendation);
}

// ============================================================
// 鼠标离开按钮时触发
// ============================================================
void RouteButton::leaveEvent(QEvent *event)
{
    QPushButton::leaveEvent(event);
    
    // 恢复普通样式
    updateStyle(false);
    
    // 发出信号，通知外部"用户不再查看这条路线"
    emit routeUnhovered();
}

// ============================================================
// 更新按钮样式
// isHovered: true=鼠标悬停, false=正常状态
// ============================================================
void RouteButton::updateStyle(bool isHovered)
{
    // ---- 定义颜色变量 ----
    QString baseColor = "#FFFFFF";       // 底色：白色
    QString hoverColor = "#F0F0F2";      // 悬停色：浅灰
    QString borderColor = "transparent"; // 边框色：透明
    QString textColor = "#1C1C1E";       // 文字色：深灰

    // ---- 如果会迟到，换成红色系 ----
    if (recommendation.isLate)
    {
        baseColor = "#FFF5F5";    // 浅红色底
        hoverColor = "#FFEBEB";   // 悬停时更红一点
        borderColor = "#FFE0E0";  // 红色边框
        textColor = "#C62828";    // 红色文字
    }

    // ---- 根据状态设置样式表 ----
    QString style;
    
    if (isHovered)
    {
        // 悬停时的样式
        style = QString(
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
    }
    else
    {
        // 正常状态的样式
        style = QString(
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
    }
    
    setStyleSheet(style);
}