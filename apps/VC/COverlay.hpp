// COverlay.hpp
// Philip Allgaier 2024 May
#pragma once

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <opencv2/imgproc.hpp>

#include "COverlayHandler.hpp"

namespace ChaoVis
{
class COverlayGraphicsItem : public QGraphicsItem
{
    public:
        COverlayGraphicsItem(QWidget* parent = nullptr);

        void setPoints(OverlaySliceData input) { points = input; }
        void setPen(QPen input) { pen = input; }
        void setBrush(QBrush input) { brush = input; }

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr);
        auto boundingRect() const -> QRectF;

    protected:
        OverlaySliceData points;
        QRect rect;
        QPen pen;
        QBrush brush;
};

}  // namespace ChaoVis
