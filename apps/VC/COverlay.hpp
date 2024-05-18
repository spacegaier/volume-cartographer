// COverlay.hpp
// Philip Allgaier 2024 May
#pragma once

#include <QGraphicsItem>
#include <QGraphicsScene>


namespace ChaoVis
{

class COverlayGraphicsItem : public QGraphicsItem
{
    Q_OBJECT

    public:
        COverlayGraphicsItem(QWidget* parent = nullptr);

        void setPoints(std::vector<cv::Vec2d> input) { points = input; }
        void setPen(QPen input) { pen = input; }
        void setBrush(QBrush input) { brush = input; }
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr);

    protected:
        std::vector<cv::Vec2d> points;
        QPen pen;
        QBrush brush;
};

}  // namespace ChaoVis
