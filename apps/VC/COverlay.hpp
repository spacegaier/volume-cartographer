// COverlay.hpp
// Philip Allgaier 2024 May
#pragma once

#include <QGraphicsItem>
#include <QGraphicsView>
#include <opencv2/imgproc.hpp>

#include "COverlayHandler.hpp"

namespace ChaoVis
{
class COverlayGraphicsItem : public QGraphicsItem
{
    public:
        COverlayGraphicsItem(QGraphicsView* graphicsView, OverlaySliceData points, QRect sceneRect, QWidget* parent = nullptr);

        void setPen(QPen input) { pen = input; }
        void setBrush(QBrush input) { brush = input; }

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr);
        auto boundingRect() const -> QRectF;
        auto type() const -> int { return 70000; }

    protected:
        QGraphicsView* view;
        OverlaySliceData points;
        QRect sceneRect;
        QRect intBoundingRect;
        QPen pen;
        QBrush brush;
};

}  // namespace ChaoVis
