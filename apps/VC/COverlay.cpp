// COverlay.hpp
// Philip Allgaier 2024 May
#include "COverlay.hpp"

#include <QPainter>

using namespace ChaoVis;

COverlayGraphicsItem::COverlayGraphicsItem(QWidget* parent)
{
    setCacheMode(QGraphicsItem::DeviceCoordinateCache);
}

void COverlayGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    const int pointWidth = 4;

    painter->setPen(pen);
    painter->setBrush(brush);    

    int left, right, top, bottom;
    int boundLeft, boundRight, boundTop, boundBottom;

    for (auto point : points) {
        left = point[0] - pointWidth / 2;
        top = point[1] - pointWidth / 2;
        right = left + pointWidth;
        bottom = top + pointWidth;

        painter->drawEllipse(left, top, pointWidth, pointWidth);

        if (left < boundLeft) {
            boundLeft = left;
        }

        if (right > boundRight) {
            boundRight = right;
        }

        if (top < boundTop) {
            boundTop = top;
        }

        if (bottom > boundBottom) {
            boundBottom = bottom;
        }
    }

    // rect.setTopLeft(QPoint(boundLeft, boundTop));
    rect.setTopLeft(QPoint(0, 0));
    rect.setBottomRight(QPoint(2*boundRight, 2*boundBottom));
}

auto COverlayGraphicsItem::boundingRect() const -> QRectF
{
    return rect;
}