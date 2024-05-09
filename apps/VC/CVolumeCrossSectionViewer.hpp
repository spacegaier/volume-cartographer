// CVolumeCrossSectionViewer.h
// Philip Allgaier 2024 May
#pragma once

#include "CImageViewer.hpp"

#include "vc/core/types/Volume.hpp"

namespace vc = volcart;

#define CROSS_SECTION_COLOR_TOP 70, 90, 180
#define CROSS_SECTION_COLOR_SIDE 255, 180, 30
#define CROSS_SECTION_COLOR_FRONT 255, 120, 110

namespace ChaoVis
{
class CVolumeViewerWithCurve;

class CVolumeCrossSectionViewer : public QWidget
{
    Q_OBJECT

public:
    CVolumeCrossSectionViewer(CVolumeViewerWithCurve* viewer, QWidget* parent = 0);

    void initViewer();
    void updateImage(vc::VolumeAxis axis);
    void setVolume(vc::Volume::Pointer volume);
    void setcrossSectionIndexTop(const int index) { crossSectionIndexTop = index; drawCrossSectionMarkers(); }
    void drawCrossSectionMarkers();

    auto getViewerForAxis(vc::VolumeAxis axis) -> CImageViewer*;

    void onLoadAnySlice(int slice, vc::VolumeAxis axis);
    void onLoadPrevSliceShift(int shift, vc::VolumeAxis axis);
    void onLoadNextSliceShift(int shift, vc::VolumeAxis axis);

protected:
    CVolumeViewerWithCurve* volumeCurveViewer{nullptr};
    CImageViewer* volumeViewerCrossSide;
    CImageViewer* volumeViewerCrossFront;

    vc::Volume::Pointer currentVolume{nullptr};

    int crossSectionIndexTop{0};
    int crossSectionIndexSide{0};
    int crossSectionIndexFront{0};
    QGraphicsRectItem* crossSideViewTopMarker{nullptr};
    QGraphicsRectItem* crossSideViewFrontMarker{nullptr};
    QGraphicsRectItem* crossFrontViewTopMarker{nullptr};
    QGraphicsRectItem* crossFrontViewSideMarker{nullptr};

    bool initialized{false};

};  // class CVolumeCrossSectionViewer

}  // namespace ChaoVis
