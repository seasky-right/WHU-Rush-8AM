#include "WeatherOverlay.h"
#include <QtCore/QRandomGenerator>
#include <cmath>
#include <QtGui/QPainter> 

// =========================================================
//  构造与析构
// =========================================================

WeatherOverlay::WeatherOverlay(QGraphicsItem* parent)
    : QGraphicsObject(parent)
{
    // 1. 设置不接受鼠标事件 (点击穿透)
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(false);
    
    // 2. 层级最高，画在最上面
    setZValue(9999); 
    
    // 3. 忽略缩放 (保证雨滴大小恒定，不随地图缩放而变大变小)
    setFlag(QGraphicsItem::ItemIgnoresTransformations); 

    // 4. 启动物理引擎定时器
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &WeatherOverlay::onPhysicsUpdate);
}

WeatherOverlay::~WeatherOverlay() {
    if (m_timer->isActive()) m_timer->stop();
}

// =========================================================
//  设置与状态控制
// =========================================================

void WeatherOverlay::setOverlayRect(const QRectF& rect) {
    m_rect = rect;
    // 如果窗口大小变了，重置所有粒子位置，防止粒子卡在屏幕外
    if (m_currentType != OverlayType::Sunny) {
        for (auto& p : m_particles) {
            resetParticle(p, true); 
        }
    }
}

void WeatherOverlay::setWeatherType(OverlayType type) {
    // 强制更新状态
    m_currentType = type;
    m_particles.clear();
    
    // 晴天模式
    if (m_currentType == OverlayType::Sunny) {
        m_maxParticles = 0;
        m_timer->stop();
        // 晴天也要显示，因为要画“提亮滤镜”
        this->setVisible(true); 
    } 
    // 雨雪模式
    else {
        this->setVisible(true);
        if (m_currentType == OverlayType::Rainy) {
            m_maxParticles = 450; // 雨滴数量
            m_particles.resize(m_maxParticles);
            for(auto& p : m_particles) resetParticle(p, true);
            m_timer->start(30);   // 30ms 刷新一次
        } 
        else if (m_currentType == OverlayType::Snowy) {
            m_maxParticles = 250; // 雪花数量
            m_particles.resize(m_maxParticles);
            for(auto& p : m_particles) resetParticle(p, true);
            m_timer->start(40);   // 40ms 刷新一次 (雪下得慢一点)
        }
    }
    update(); // 触发重绘
}

QRectF WeatherOverlay::boundingRect() const {
    // 返回视口大小，如果没有设置则给一个默认大值，防止不刷新
    return m_rect.isEmpty() ? QRectF(0,0,2000,2000) : m_rect;
}

// =========================================================
//  物理引擎 (Physics)
// =========================================================

void WeatherOverlay::resetParticle(WeatherParticle& p, bool randomY) {
    if (m_rect.width() <= 1) return; // 防止除零错误
    
    // X轴：随机分布在整个屏幕宽度
    p.x = QRandomGenerator::global()->bounded((int)m_rect.width());
    
    // Y轴：初始化时铺满全屏，运行时从顶部落下
    if (randomY) {
        p.y = QRandomGenerator::global()->bounded((int)m_rect.height());
    } else {
        p.y = -20.0; // 从屏幕上方一点点开始
    }

    if (m_currentType == OverlayType::Rainy) {
        // 雨滴物理参数
        p.speedY = 15.0 + QRandomGenerator::global()->bounded(10); // 下落速度 15-25
        p.speedX = -2.0; // 风向向左
        p.size = 10.0 + QRandomGenerator::global()->bounded(10);   // 雨滴长度
        p.opacity = 0.5 + (QRandomGenerator::global()->bounded(30) / 100.0); // 透明度
    } 
    else {
        // 雪花物理参数
        p.speedY = 1.0 + (QRandomGenerator::global()->bounded(15) / 10.0); // 下落速度 1.0-2.5
        p.speedX = 0; 
        p.size = 2.0 + (QRandomGenerator::global()->bounded(20) / 10.0); // 雪花大小
        p.opacity = 0.7 + (QRandomGenerator::global()->bounded(30) / 100.0);
        p.swayPhase = QRandomGenerator::global()->bounded(628) / 100.0; // 摇摆相位
    }
}

void WeatherOverlay::onPhysicsUpdate() {
    if (m_currentType == OverlayType::Sunny) return;

    if (m_currentType == OverlayType::Rainy) updateRainPhysics();
    else if (m_currentType == OverlayType::Snowy) updateSnowPhysics();
    
    update(); // 通知 Qt 重绘
}

void WeatherOverlay::updateRainPhysics() {
    for (auto& p : m_particles) {
        p.y += p.speedY;
        p.x += p.speedX;
        
        // 边界检测：超出屏幕底部或左侧，重置
        if (p.y > m_rect.height() || p.x < 0) {
            resetParticle(p);
        }
    }
}

void WeatherOverlay::updateSnowPhysics() {
    for (auto& p : m_particles) {
        p.y += p.speedY;
        
        // 雪花摇摆算法：x += sin(phase)
        p.swayPhase += 0.05; 
        p.x += std::sin(p.swayPhase) * 0.8; 
        
        if (p.y > m_rect.height()) {
            resetParticle(p);
        }
    }
}

// =========================================================
//  渲染逻辑 (Rendering)
// =========================================================

void WeatherOverlay::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) {
    Q_UNUSED(option);
    Q_UNUSED(widget);
    
    // 1. 绘制环境滤镜 (控制明暗)
    if (m_currentType == OverlayType::Sunny) {
        // 晴天滤镜：白色，透明度30 (提亮地图)
        painter->fillRect(m_rect, QColor(255, 255, 255, 30)); 
    }
    /*else {
        // 雨雪滤镜：黑色，透明度80 (压暗地图，营造氛围)
        //painter->fillRect(m_rect, QColor(0, 0, 0, 80));
    }*/

    if (m_currentType == OverlayType::Sunny) return;

    // 2. 绘制粒子
    painter->setRenderHint(QPainter::Antialiasing);

    if (m_currentType == OverlayType::Rainy) drawRain(painter);
    else if (m_currentType == OverlayType::Snowy) drawSnow(painter);
}

void WeatherOverlay::drawRain(QPainter* painter) {
    QPen pen;
    pen.setWidth(2);
    pen.setCapStyle(Qt::RoundCap);
    
    for (const auto& p : m_particles) {
        QColor c(200, 220, 255); // 淡蓝色雨滴
        c.setAlphaF(p.opacity);
        pen.setColor(c);
        painter->setPen(pen);
        
        // 画线：考虑风向 speedX 产生的倾斜
        painter->drawLine(QPointF(p.x, p.y), QPointF(p.x + p.speedX * 2, p.y + p.size));
    }
}

void WeatherOverlay::drawSnow(QPainter* painter) {
    painter->setPen(Qt::NoPen);
    
    for (const auto& p : m_particles) {
        QColor c(255, 255, 255); // 纯白雪花
        c.setAlphaF(p.opacity);
        painter->setBrush(c);
        
        // 画圆
        painter->drawEllipse(QPointF(p.x, p.y), p.size, p.size);
    }
}