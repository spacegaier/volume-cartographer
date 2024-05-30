// CImageViewer.hpp
// Philip Allgaier 2023 November
#pragma once

#include <QtWidgets>
#include <QGraphicsView>
#include <QGraphicsScene>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

namespace ChaoVis
{

class CImageViewerView : public QGraphicsView
{
    Q_OBJECT

    public:
        CImageViewerView(QWidget* parent = nullptr);

        void setup();

        void keyPressEvent(QKeyEvent* event) override;
        void keyReleaseEvent(QKeyEvent* event) override;

        bool isRangeKeyPressed() { return rangeKeyPressed; }
        bool isCurvePanKeyPressed() { return curvePanKeyPressed; }
        bool isRotateKyPressed() { return rotateKeyPressed; }

        void showTextAboveCursor(const QString& value, const QString& label, const QColor& color);
        void hideTextAboveCursor();

        void showCurrentImpactRange(int range);
        void showCurrentScanRange(int range);
        void showCurrentImageIndex(int slice, bool highlight);

    protected:
        bool rangeKeyPressed{false};
        bool curvePanKeyPressed{false};
        bool rotateKeyPressed{false};

        QGraphicsTextItem* textAboveCursor;
        QGraphicsRectItem* backgroundBehindText;
        QTimer* timerTextAboveCursor;
};

class CImageViewer : public QWidget
{
    Q_OBJECT

public:
    CImageViewer(QWidget* parent = 0);
    ~CImageViewer(void);
    virtual void SetButtonsEnabled(bool state);

    virtual void SetImage(const QImage& nSrc, const QPoint pos = {0, 0});
    void SetImageIndex(int imageIndex)
    {
        fImageIndex = imageIndex;
        fImageIndexSpin->setValue(imageIndex);
        UpdateButtons();
    }
    auto GetImageIndex() const -> int { return fImageIndex; }
    void SetNumImages(int num);
    auto GetNumImages() const -> int { return fImageIndexSpin->maximum(); }
    void SetScanRange(int scanRange);
    auto GetImageSize() const -> QSize { return fBaseImageItem->pixmap().size(); }
    void ScaleImage(double nFactor);
    auto GetScene() const -> QGraphicsScene* { return fScene; }
    auto GetView() const -> CImageViewerView* { return fGraphicsView; }
    auto GetImage() const -> QImage* { return fImgQImage; }

    void SetRotation(int degress);
    void Rotate(int delta);
    void ResetRotation();

    void ResetView();
    void ScheduleChunkUpdate();

    virtual bool CanChangeImage() const { return fPrevBtn->isEnabled(); }
    virtual bool ShouldHighlightImageIndex(const int imageIndex) { return false; }

    void OnZoomInClicked(void);
    void OnZoomOutClicked(void);
    void OnResetClicked(void);
    void OnNextClicked(void);
    void OnPrevClicked(void);
    void OnImageIndexSpinChanged(void);
    void OnImageRotationSpinChanged(void);

protected:
    bool eventFilter(QObject* watched, QEvent* event);

signals:
    void SendSignalOnNextImageShift(int shift);
    void SendSignalOnPrevImageShift(int shift);
    void SendSignalOnLoadAnyImage(int nImageIndex);
    void SendSignalStatusMessageAvailable(QString text, int timeout);
    void SendSignalImpactRangeUp(void);
    void SendSignalImpactRangeDown(void);

protected:
    void CenterOn(const QPointF& point);
    virtual void UpdateButtons(void);
    void AdjustScrollBar(QScrollBar* nScrollBar, double nFactor);
    void ScrollToCenter(cv::Vec2f pos);
    auto GetScrollPosition() const -> cv::Vec2f;
    auto CleanScrollPosition(cv::Vec2f pos) const -> cv::Vec2f;

    virtual void mousePressEvent(QMouseEvent* event);
    virtual void mouseMoveEvent(QMouseEvent* event);
    virtual void mouseReleaseEvent(QMouseEvent* event);

protected:
    // widget components
    CImageViewerView* fGraphicsView;
    QGraphicsScene* fScene;

    QPushButton* fNextBtn;
    QPushButton* fPrevBtn;
    QScrollArea* fScrollArea;
    QPushButton* fZoomInBtn;
    QPushButton* fZoomOutBtn;
    QPushButton* fResetBtn;
    QSpinBox* fImageIndexSpin;
    QSpinBox* fImageRotationSpin;
    QHBoxLayout* fButtonsLayout;
    QSpacerItem* fSpacer;

    // data
    QImage* fImgQImage;
    double fScaleFactor;
    int fImageIndex;
    int fScanRange;
    // Required to be able to reset the rotation without also resetting the scaling
    int currentRotation{0};

    // user settings
    bool fCenterOnZoomEnabled;
    int fScrollSpeed{-1};
    bool fSkipImageFormatConv;

    // pan handling
    bool wantsPanning{false};
    bool isPanning{false};
    bool rightPressed{false};
    int panStartX, panStartY;

    QGraphicsPixmapItem* fBaseImageItem;
    QTimer* timerChunkUpdate;
};  // class CImageViewer

}  // namespace ChaoVis
