// COverlay.hpp
// Philip Allgaier 2024 May
#include "COverlay.hpp"

#include <QPainter>

#include <iostream>

using namespace ChaoVis;

COverlayGraphicsItem::COverlayGraphicsItem(QGraphicsView* graphicsView, OverlaySliceData points, QRect sceneRect, QWidget* parent) : 
    QGraphicsItem(), view(graphicsView), points(points), sceneRect(sceneRect)
{
    // setCacheMode(QGraphicsItem::ItemCoordinateCache);
}

void COverlayGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    const int pointWidth = 4;

    painter->setPen(pen);
    painter->setBrush(brush);

    int left = 0, right = 0, top = 0, bottom = 0;
    int boundLeft = 0, boundRight = 0, boundTop = 0, boundBottom = 0;

    const auto sceneRectCenter = sceneRect.topLeft() + (sceneRect.bottomRight() - sceneRect.topLeft()) / 2;
    // std::cout << "Center: " << sceneRectCenter.x() << "|" << sceneRectCenter.y() << std::endl;
    int count = 0;

    for (auto point : points) {

        if (!sceneRect.contains(point[0], point[1])) {
            continue;
        }

        left = point[0] - sceneRectCenter.x();
        top = point[1] - sceneRectCenter.y();
        right = left + pointWidth;
        bottom = top + pointWidth;

        painter->drawEllipse(left, top, pointWidth, pointWidth);
        count++;

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

    intBoundingRect.setTopLeft(QPoint(boundLeft, boundTop));
    intBoundingRect.setBottomRight(QPoint(boundRight, boundBottom));

    // std::cout << "Bounding Rect: " << intBoundingRect.left() << "|" << intBoundingRect.top() << "|"
    //           << intBoundingRect.right() << "|" << intBoundingRect.bottom() << std::endl;
    std::cout << "-----------" << std::endl;
    std::cout << "Paint : " << count << std::endl;
}

auto COverlayGraphicsItem::boundingRect() const -> QRectF
{
    return intBoundingRect;
}