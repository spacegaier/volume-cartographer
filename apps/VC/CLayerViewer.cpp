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

    progressBar = new QProgressBar(this);
    progressBar->setMinimumWidth(200);
    fButtonsLayout->addWidget(progressBar);
    fButtonsLayout->removeItem(fSpacer);
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
        if (map.pos[2] > sliceIndex - 0.3 &&
            map.pos[2] < sliceIndex + 0.3) {
            curve.InsertPoint(Vec2<double>(map.x, map.y));
        }
    }

    for (auto pt : curve.GetPoints()) {
        QGraphicsEllipseItem* newEllipse = fScene->addEllipse(pt[0], pt[1], 1, 1, QPen(Qt::red), QBrush(Qt::red));
    }
}

void CLayerViewer::setProgress(int progress) {
    if(progress == 100) {
        progressBar->setValue(0);
    } else {
        progressBar->setValue(progress);
    }
}

void CLayerViewer::setProgressText(const QString& text) {
    progressBar->setFormat(text);
}