#ifndef ROUTEBUTTON_H
#define ROUTEBUTTON_H

// Module-qualified Qt includes
#include <QtWidgets/QPushButton>
#include <QtCore/QEvent>
#include <QtGui/QEnterEvent>
#include "../model/PathRecommendation.h"

class RouteButton : public QPushButton
{
    Q_OBJECT

public:
    explicit RouteButton(const PathRecommendation& rec, QWidget *parent = nullptr);

signals:
    // 自定义信号，用于通知 MainWindow 进行地图预览
    void routeHovered(const PathRecommendation& recommendation);
    void routeUnhovered();

protected:
    // 鼠标移入/移出事件重写
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    PathRecommendation recommendation;
    void updateStyle(bool isHovered);
};

#endif // ROUTEBUTTON_H