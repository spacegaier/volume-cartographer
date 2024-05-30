// CVolumeCrossSectionViewer.cpp
// Philip Allgaier 2024 May
#include "CVolumeCrossSectionViewer.hpp"

#include "CVolumeViewerWithCurve.hpp"
#include "UDataManipulateUtils.hpp"

#include <algorithm>

using namespace ChaoVis;
namespace vc = volcart;

// Constructor
CVolumeCrossSectionViewer::CVolumeCrossSectionViewer(CVolumeViewerWithCurve* viewer, QWidget* parent)
    : QWidget(parent), volumeCurveViewer(viewer)
{
    volumeViewerCrossSide = new CImageViewer(this);
    volumeViewerCrossSide->SetButtonsEnabled(true);
    volumeViewerCrossFront = new CImageViewer(this);
    volumeViewerCrossFront->SetButtonsEnabled(true);
    setLayout(new QVBoxLayout(this));
    layout()->addWidget(volumeViewerCrossSide);
    layout()->addWidget(volumeViewerCrossFront);

    connect(
        volumeViewerCrossSide, &CImageViewer::SendSignalOnNextImageShift, this,
        [this](int shift){ onLoadNextSliceShift(shift, vc::VolumeAxis::X); });
    connect(
        volumeViewerCrossSide, &CImageViewer::SendSignalOnPrevImageShift, this,
        [this](int shift){ onLoadPrevSliceShift(shift, vc::VolumeAxis::X); });
    connect(
        volumeViewerCrossSide, &CImageViewer::SendSignalOnLoadAnyImage, this,
        [this](int index){ onLoadAnySlice(index, vc::VolumeAxis::X); });

    connect(
        volumeViewerCrossFront, &CImageViewer::SendSignalOnNextImageShift, this,
        [this](int shift){ onLoadNextSliceShift(shift, vc::VolumeAxis::Y); });
    connect(
        volumeViewerCrossFront, &CImageViewer::SendSignalOnPrevImageShift, this,
        [this](int shift){ onLoadPrevSliceShift(shift, vc::VolumeAxis::Y); });
    connect(
        volumeViewerCrossFront, &CImageViewer::SendSignalOnLoadAnyImage, this,
        [this](int index){ onLoadAnySlice(index, vc::VolumeAxis::Y); });

    volumeViewerCrossSide->setStyleSheet(QString("QGraphicsView { border: 3px solid %1; }").arg(QColor(CROSS_SECTION_COLOR_SIDE).name()));
    volumeViewerCrossFront->setStyleSheet(QString("QGraphicsView { border: 3px solid %1; }").arg(QColor(CROSS_SECTION_COLOR_FRONT).name()));
}

void CVolumeCrossSectionViewer::initViewer()
{
    updateImage(vc::VolumeAxis::X); 
    updateImage(vc::VolumeAxis::Y); 
    // Set a reasonable initial size for the dock
    auto size = volumeViewerCrossSide->GetImageSize();
    auto ratio = (float)size.height() / size.width();
    auto factor = 400.f / size.width();
    auto dockWidth = std::clamp((int)(factor * size.width()), 200, 400);
    auto dockSize = QSize({dockWidth, std::clamp((int)(dockWidth * ratio), 400, 600)});
    volumeViewerCrossSide->setBaseSize(dockSize);
    volumeViewerCrossSide->ScaleImage(factor);
    volumeViewerCrossFront->ScaleImage(factor);

    initialized = true;

    drawCrossSectionMarkers();
}

void CVolumeCrossSectionViewer::setVolume(vc::Volume::Pointer volume)
{
    currentVolume = volume;
    volumeViewerCrossSide->SetNumImages(currentVolume->sliceWidth());
    volumeViewerCrossFront->SetNumImages(currentVolume->sliceHeight());
    volumeViewerCrossSide->SetImageIndex(currentVolume->sliceWidth() / 2);
    volumeViewerCrossFront->SetImageIndex(currentVolume->sliceHeight() / 2);
    crossSectionIndexSide = currentVolume->sliceWidth() / 2;
    crossSectionIndexFront = currentVolume->sliceHeight() / 2;

    drawCrossSectionMarkers();
}

auto CVolumeCrossSectionViewer::getViewerForAxis(vc::VolumeAxis axis) -> CImageViewer*
{
    return (axis == vc::VolumeAxis::X) ? volumeViewerCrossSide : volumeViewerCrossFront;
}

void CVolumeCrossSectionViewer::onLoadAnySlice(int slice, vc::VolumeAxis axis)
{
    if (slice >= 0 && slice < getViewerForAxis(axis)->GetNumImages()) {
        getViewerForAxis(axis)->SetImageIndex(slice);
        updateImage(axis);
    }

    if (axis == vc::VolumeAxis::X) {
        volumeCurveViewer->SetcrossSectionIndexSide(getViewerForAxis(axis)->GetImageIndex());
        crossSectionIndexSide = slice;
    } else if (axis == vc::VolumeAxis::Y) {
        volumeCurveViewer->SetcrossSectionIndexFront(getViewerForAxis(axis)->GetImageIndex());
        crossSectionIndexFront = slice;
    }
    drawCrossSectionMarkers();
}

void CVolumeCrossSectionViewer::onLoadNextSliceShift(int shift, vc::VolumeAxis axis)
{
    auto current = getViewerForAxis(axis)->GetImageIndex();
    if (current + shift >= getViewerForAxis(axis)->GetNumImages()) {
        shift = getViewerForAxis(axis)->GetNumImages() - current - 1;
    }

    if (shift != 0) {
        getViewerForAxis(axis)->SetImageIndex(current + shift);
        updateImage(axis);
    }

    if (axis == vc::VolumeAxis::X) {
        volumeCurveViewer->SetcrossSectionIndexSide(getViewerForAxis(axis)->GetImageIndex());
        crossSectionIndexSide = current + shift;
    } else if (axis == vc::VolumeAxis::Y) {
        volumeCurveViewer->SetcrossSectionIndexFront(getViewerForAxis(axis)->GetImageIndex());
        crossSectionIndexFront = current + shift;
    }
    drawCrossSectionMarkers();
}

void CVolumeCrossSectionViewer::onLoadPrevSliceShift(int shift, vc::VolumeAxis axis)
{

    auto current = getViewerForAxis(axis)->GetImageIndex();
    if (current - shift < 0) {
        shift = current;
    }

    if (shift != 0) {
        getViewerForAxis(axis)->SetImageIndex(current - shift);
        updateImage(axis);
    }

    if (axis == vc::VolumeAxis::X) {
        volumeCurveViewer->SetcrossSectionIndexSide(getViewerForAxis(axis)->GetImageIndex());
        crossSectionIndexSide = current + shift;
    } else if (axis == vc::VolumeAxis::Y) {
        volumeCurveViewer->SetcrossSectionIndexFront(getViewerForAxis(axis)->GetImageIndex());
        crossSectionIndexFront = current + shift;
    }
    drawCrossSectionMarkers();
}

// Update image for a given axis
void CVolumeCrossSectionViewer::updateImage(vc::VolumeAxis axis)
{
    if(axis != vc::VolumeAxis::X && axis != vc::VolumeAxis::Y) {
        return;
    }
    
    QImage aImgQImage;
    cv::Mat aImgMat;
    auto polygon = getViewerForAxis(axis)->GetView()->mapToScene(getViewerForAxis(axis)->GetView()->viewport()->rect());
    QRect rect(std::max(0, static_cast<int>(polygon.at(0).x())), 
               std::max(0, static_cast<int>(polygon.at(0).y())), 
               polygon.at(2).x() - polygon.at(0).x(), polygon.at(2).y() - polygon.at(0).y());

    if (currentVolume != nullptr) {  
        aImgMat = currentVolume->getSliceDataDefault(getViewerForAxis(axis)->GetImageIndex(),
            cv::Rect(rect.x(), rect.y(), rect.width(), rect.height()),
            axis);
    } else {
        aImgMat = cv::Mat::zeros(10, 10, CV_8UC1);
    }

    if (aImgMat.isContinuous() && aImgMat.type() == CV_16U) {
        // create QImage directly backed by cv::Mat buffer
        aImgQImage = QImage(aImgMat.ptr(), aImgMat.cols, aImgMat.rows, aImgMat.step, QImage::Format_Grayscale16);
    } else {
        aImgQImage = Mat2QImage(aImgMat);
    }

    getViewerForAxis(axis)->SetImage(aImgQImage, QPoint(rect.x(), rect.y()));
}

void CVolumeCrossSectionViewer::drawCrossSectionMarkers()
{
    static const int width = 8;
    
    if (!initialized) {
        return;
    }

    auto topMarkerRectForSide = QRect(0, crossSectionIndexTop - (width / 2), 
        volumeViewerCrossSide->GetImage()->size().width(), width);
    auto topMarkerRectForFront = QRect(0, crossSectionIndexTop - (width / 2), 
        volumeViewerCrossFront->GetImage()->size().width(), width);
    auto sideMarkerRect = QRect(crossSectionIndexSide - (width / 2), 0,
        width, volumeViewerCrossFront->GetImage()->size().height());
    auto frontMarkerRect = QRect(crossSectionIndexFront - (width / 2), 0,
        width, volumeViewerCrossSide->GetImage()->size().height());

    if (!crossSideViewTopMarker) {
        // Side view
        auto color = QColor(CROSS_SECTION_COLOR_TOP);
        color.setAlpha(128);
        crossSideViewTopMarker = volumeViewerCrossSide->GetScene()->addRect(topMarkerRectForSide, QPen(color), QBrush(color));
        crossSideViewTopMarker->setZValue(999);
        color = QColor(CROSS_SECTION_COLOR_FRONT);
        color.setAlpha(128);
        crossSideViewFrontMarker = volumeViewerCrossSide->GetScene()->addRect(frontMarkerRect, QPen(color), QBrush(color));
        crossSideViewFrontMarker->setZValue(999);

        // Front view
        color = QColor(CROSS_SECTION_COLOR_TOP);
        color.setAlpha(128);
        crossFrontViewTopMarker = volumeViewerCrossFront->GetScene()->addRect(topMarkerRectForFront, QPen(color), QBrush(color));
        crossFrontViewTopMarker->setZValue(999);
        color = QColor(CROSS_SECTION_COLOR_SIDE);
        color.setAlpha(128);
        crossFrontViewSideMarker = volumeViewerCrossFront->GetScene()->addRect(sideMarkerRect, QPen(color), QBrush(color));
        crossFrontViewSideMarker->setZValue(999);
    } else {
        // Side view
        crossSideViewTopMarker->setRect(topMarkerRectForSide);
        crossSideViewFrontMarker->setRect(frontMarkerRect);

        // Front view
        crossFrontViewTopMarker->setRect(topMarkerRectForFront);
        crossFrontViewSideMarker->setRect(sideMarkerRect);
    }
}