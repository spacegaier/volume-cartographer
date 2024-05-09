// CVolumeViewerWithCurve.h
// Chao Du 2015 April
#pragma once

#include <QCheckBox>
#include <vector>
#include <opencv2/core.hpp>
#include "CBSpline.hpp"
#include "CVolumeViewer.hpp"
#include "CXCurve.hpp"
#include "ColorFrame.hpp"
#include "SegmentationStruct.hpp"

#include <QList>
#include <QGraphicsEllipseItem>
#include <QGraphicsView>
#include <QGraphicsScene>

namespace ChaoVis
{

// REVISIT - NOTE - since there are two modes, edit and draw, for the
// application, we need to add corresponding modes to the widget, too.

class CVolumeViewerWithCurve : public CVolumeViewer
{

    Q_OBJECT

public:
    CVolumeViewerWithCurve(std::unordered_map<std::string, SegmentationStruct>& nSegStructMapRef);
    ~CVolumeViewerWithCurve();

    // for drawing mode
    void SetSplineCurve(CBSpline& nCurve);
    void UpdateSplineCurve(void);
    void ResetSplineCurve(void) { fControlPoints.clear(); }
    // for editing mode
    void SetIntersectionCurve(CXCurve& nCurve);
    void SetImpactRange(int nImpactRange);
    void SetSliceIndexToolStart(int index) { sliceIndexToolStart = index; }
    void ReturnToSliceIndexToolStart();
    bool ShouldHighlightImageIndex(const int imageIndex);
    void SetcrossSectionIndexSide(const int index) { crossSectionIndexSide = index; DrawCrossSectionMarkers(); }
    void SetcrossSectionIndexFront(const int index) { crossSectionIndexFront = index; DrawCrossSectionMarkers(); }

    void UpdateView();
    void SetShowCurve(bool b) { showCurve = b; }
    void toggleShowCurveBox();

    void setButtonsEnabled(bool state);

protected:
    bool eventFilter(QObject* watched, QEvent* event);
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void UpdateButtons(void);

private slots:
    void OnShowCurveStateChanged(int state);
    void panAlongCurve(double speed, bool forward);

private:
    void WidgetLoc2ImgLoc(const cv::Vec2f& nWidgetLoc, cv::Vec2f& nImgLoc);

    std::pair<int, std::string> SelectPointOnCurves(const cv::Vec2f& nPt, bool rightClick, bool selectGlobally=false);

    void DrawIntersectionCurve();
    void DrawControlPoints();
    void DrawCrossSectionMarkers();

signals:
    void SendSignalPathChanged(std::string, PathChangePointVector before, PathChangePointVector after);
    void SendSignalAnnotationChanged(void);

private:
    // for interaction
    QTimer *timer;
    Qt::MouseButton lastPressedSideButton{Qt::NoButton};
    cv::Vec2f scrollPositionModifier{cv::Vec2f(0.0, 0.0)};

    // for drawing
    ColorFrame* colorSelector{nullptr};
    ColorFrame* colorSelectorCompute{nullptr};
    ColorFrame* colorSelectorHighlight{nullptr};
    ColorFrame* colorSelectorManual{nullptr};
    QCheckBox* fShowCurveBox;
    bool showCurve;
    int fwdBackMsJump;
    std::unordered_map<std::string, SegmentationStruct>& fSegStructMapRef;
    CBSpline* fSplineCurveRef;
    std::vector<cv::Vec2f> fControlPoints;

    // for editing
    CXCurve* fIntersectionCurveRef;
    int fSelectedPointIndex;
    std::string fSelectedSegID;
    PathChangePointVector pathChangeBefore;
    std::set<int> movedPointIndexSet; // set of points that are currently grabbed and have been moved

    bool fVertexIsChanged;
    bool curveGrabbed{false};
    QPointF fLastPos;  // last mouse position on the image
    int fImpactRange;  // how many points a control point movement can affect

    // cross section integration
    int crossSectionIndexSide;
    int crossSectionIndexFront;
    QGraphicsRectItem* crossSectionMarkerRectSide{nullptr};
    QGraphicsRectItem* crossSectionMarkerRectFront{nullptr};

};  // class CVolumeViewerWithCurve

}  // namespace ChaoVis
