// CLayerViewer.cpp
// Philip Allgaier 2023 November
#include "CLayerViewer.hpp"

using namespace ChaoVis;

// Constructor
CLayerViewer::CLayerViewer(QWidget* parent)
    : CImageViewer(parent)
{
    fNextBtn->setText(tr("Next Layer"));
    fPrevBtn->setText(tr("Previous Layer"));
}

void CLayerViewer::showCurveForSlice(int sliceIndex) {
    QList<QGraphicsItem*> allItems = fScene->items();
    for(QGraphicsItem *item : allItems)
    {
        if (dynamic_cast<QGraphicsEllipseItem*>(item) || dynamic_cast<QGraphicsLineItem*>(item))
        {
            fScene->removeItem(item);
            delete item;
        }
    }

    CXCurve curve;
    for (auto map : ppm.getMappings()) {
        if (std::round(map.pos[2]) == sliceIndex) {
            curve.InsertPoint(Vec2<double>(map.x, map.y));
        }
    }

    for (auto pt : curve.GetPoints()) {
        QGraphicsEllipseItem* newEllipse = fScene->addEllipse(pt[0], pt[1], 1, 1, QPen(Qt::red), QBrush(Qt::red));
    }
}