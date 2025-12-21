#pragma once

#include <QtWidgets/QGraphicsObject>
#include <QtCore/QTimer>
#include <QtCore/QVector>
#include <QtCore/QPointF>
#include <QtGui/QColor>
#include <QtGui/QPainter>

// 定义天气类型，用于视图层渲染
enum class OverlayType {
    Sunny,
    Rainy,
    Snowy
};

// 粒子结构体：包含位置、速度、大小、透明度等物理属性
struct WeatherParticle {
    double x, y;        // 当前坐标
    double speedY;      // 垂直下落速度
    double speedX;      // 水平漂移速度 (模拟风)
    double size;        // 粒子大小 (雨滴长度或雪花半径)
    double opacity;     // 透明度 (0.0 - 1.0)
    double swayPhase;   // 摇摆相位 (仅用于雪花 sin 运动)
};

/**
 * @brief 天气覆盖层类 (WeatherOverlay)
 * @details 该类继承自 QGraphicsObject，实现了一个基于 CPU 的轻量级粒子系统。
 * 通过重写 paint 函数在一个图层上绘制数百个粒子，避免了创建数百个 QGraphicsItem 带来的性能开销。
 * 支持雨天 (Rainy) 和 雪天 (Snowy) 的物理模拟。
 */
class WeatherOverlay : public QGraphicsObject
{
    Q_OBJECT
public:
    explicit WeatherOverlay(QGraphicsItem* parent = nullptr);
    ~WeatherOverlay() override;
    
    // 设置覆盖层的矩形范围 (通常与视口 Viewport 大小一致)
    void setOverlayRect(const QRectF& rect);
    
    // 切换天气模式
    void setWeatherType(OverlayType type);

    // QGraphicsItem 必须实现的接口
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private slots:
    // 定时器回调：更新粒子物理位置
    void onPhysicsUpdate();

private:
    QRectF m_rect;
    OverlayType m_currentType = OverlayType::Sunny;
    
    QVector<WeatherParticle> m_particles; // 粒子对象池
    QTimer* m_timer;                      // 渲染循环定时器
    
    int m_maxParticles = 0;               // 当前最大粒子数
    
    // 物理引擎辅助函数
    void resetParticle(WeatherParticle& p, bool randomY = false);
    void updateRainPhysics();
    void updateSnowPhysics();
    void drawRain(QPainter* painter);
    void drawSnow(QPainter* painter);
};