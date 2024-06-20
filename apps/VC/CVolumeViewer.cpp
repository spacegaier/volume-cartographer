// CVolumeViewer.cpp
// Chao Du 2015 April
#include "CVolumeViewer.hpp"
#include "HBase.hpp"

#include "COverlay.hpp"

using namespace ChaoVis;
using qga = QGuiApplication;

#define BGND_RECT_MARGIN 8
#define DEFAULT_TEXT_COLOR QColor(255, 255, 120)
#define ZOOM_FACTOR 1.15

// Constructor
CVolumeViewerView::CVolumeViewerView(QWidget* parent)
: QGraphicsView(parent)
{
    timerTextAboveCursor = new QTimer(this);
    connect(timerTextAboveCursor, &QTimer::timeout, this, &CVolumeViewerView::hideTextAboveCursor);
    timerTextAboveCursor->setSingleShot(true);
}

void CVolumeViewerView::setup()
{
    textAboveCursor = new QGraphicsTextItem("", 0);
    textAboveCursor->setFlag(QGraphicsItem::ItemIgnoresTransformations);
    textAboveCursor->setZValue(100);
    textAboveCursor->setVisible(false);
    textAboveCursor->setDefaultTextColor(DEFAULT_TEXT_COLOR);
    scene()->addItem(textAboveCursor);

    QFont f;
    f.setPointSize(f.pointSize() + 2);
    textAboveCursor->setFont(f);

    backgroundBehindText = new QGraphicsRectItem();
    backgroundBehindText->setFlag(QGraphicsItem::ItemIgnoresTransformations);
    backgroundBehindText->setPen(Qt::NoPen);
    backgroundBehindText->setZValue(99);
    scene()->addItem(backgroundBehindText);
}

void CVolumeViewerView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_W) {
        rangeKeyPressed = true;
        event->accept();
    } else if (event->key() == Qt::Key_R) {
        curvePanKeyPressed = true;
        event->accept();
    } else if (event->key() == Qt::Key_S) {
        rotateKeyPressed = true;
        event->accept();
    }
}

void CVolumeViewerView::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_W)  {
        rangeKeyPressed = false;
        event->accept();
    } else if (event->key() == Qt::Key_R) {
        curvePanKeyPressed = false;
        event->accept();
    } else if (event->key() == Qt::Key_S) {
        rotateKeyPressed = false;
        event->accept();
    }
}

void CVolumeViewerView::showTextAboveCursor(const QString& value, const QString& label, const QColor& color)
{
    // Without this check, when you start VC with auto-load the initial slice will not be in the center of
    // volume viewer, because during loading the initilization of the impact range slider and its callback slots
    // will already move the position/scrollbars of the viewer and therefore the image is no longer centered.
    if (!isVisible()) {
        return;
    }

    timerTextAboveCursor->start(150);

    QFontMetrics fm(textAboveCursor->font());
    QPointF p = mapToScene(mapFromGlobal(QPoint(QCursor::pos().x() + 10, QCursor::pos().y())));

    textAboveCursor->setVisible(true);
    textAboveCursor->setHtml("<b>" + value + "</b><br>" + label);
    textAboveCursor->setPos(p);
    textAboveCursor->setDefaultTextColor(color);

    backgroundBehindText->setVisible(true);
    backgroundBehindText->setPos(p);
    backgroundBehindText->setRect(0, 0, fm.horizontalAdvance((label.isEmpty() ? value : label)) + BGND_RECT_MARGIN, fm.height() * (label.isEmpty() ? 1 : 2) + BGND_RECT_MARGIN);
    backgroundBehindText->setBrush(QBrush(QColor(
        (2 * 125 + color.red())   / 3,
        (2 * 125 + color.green()) / 3,
        (2 * 125 + color.blue())  / 3,
    200)));
}

void CVolumeViewerView::hideTextAboveCursor()
{
    textAboveCursor->setVisible(false);
    backgroundBehindText->setVisible(false);
}

void CVolumeViewerView::showCurrentImpactRange(int range)
{
    showTextAboveCursor(QString::number(range), "", QColor(255, 120, 110)); // tr("Impact Range")
}

void CVolumeViewerView::showCurrentScanRange(int range)
{
    showTextAboveCursor(QString::number(range), "", QColor(160, 180, 255)); // tr("Scan Range")
}

void CVolumeViewerView::showCurrentSliceIndex(int slice, bool highlight)
{
    showTextAboveCursor(QString::number(slice), "", (highlight ? QColor(255, 50, 20) : QColor(255, 220, 30))); // tr("Slice")
}

// Constructor
CVolumeViewer::CVolumeViewer(QWidget* parent)
    : QWidget(parent)
    , fCanvas(nullptr)
    , fScrollArea(nullptr)
    , fGraphicsView(nullptr)
    , fZoomInBtn(nullptr)
    , fZoomOutBtn(nullptr)
    , fResetBtn(nullptr)
    , fNextBtn(nullptr)
    , fPrevBtn(nullptr)
    , fViewState(EViewState::ViewStateIdle)
    , fImgQImage(nullptr)
    , fBaseImageItem(nullptr)
    , fScaleFactor(1.0)
    , fImageIndex(0)
    , fScanRange(1)
{
    overlayItems.clear();

    // buttons
    fZoomInBtn = new QPushButton(tr("Zoom In"), this);
    fZoomOutBtn = new QPushButton(tr("Zoom Out"), this);
    fResetBtn = new QPushButton(tr("Reset"), this);
    fNextBtn = new QPushButton(tr("Next Slice"), this);
    fPrevBtn = new QPushButton(tr("Previous Slice"), this);

    // slice index spin box
    fImageIndexSpin = new QSpinBox(this);
    fImageIndexSpin->setMinimum(0);
    fImageIndexSpin->setEnabled(true);
    fImageIndexSpin->setMinimumWidth(100);
    connect(fImageIndexSpin, SIGNAL(editingFinished()), this, SLOT(OnImageIndexSpinChanged()));

    fImageRotationSpin = new QSpinBox(this);
    fImageRotationSpin->setMinimum(-360);
    fImageRotationSpin->setMaximum(360);
    fImageRotationSpin->setSuffix("°");
    fImageRotationSpin->setEnabled(true);
    connect(fImageRotationSpin, SIGNAL(editingFinished()), this, SLOT(OnImageRotationSpinChanged()));

    fBaseImageItem = nullptr;

    // Create graphics view
    fGraphicsView = new CVolumeViewerView(this);
    fGraphicsView->setRenderHint(QPainter::Antialiasing);
    setFocusProxy(fGraphicsView);

    // Create graphics scene
    fScene = new QGraphicsScene(this);

    // Set the scene
    fGraphicsView->setScene(fScene);
    fGraphicsView->setup();

    fGraphicsView->viewport()->installEventFilter(this);

    fOverlayHandler = new COverlayHandler(this);

    fButtonsLayout = new QHBoxLayout;
    fButtonsLayout->addWidget(fZoomInBtn);
    fButtonsLayout->addWidget(fZoomOutBtn);
    fButtonsLayout->addWidget(fResetBtn);
    fButtonsLayout->addWidget(fPrevBtn);
    fButtonsLayout->addWidget(fNextBtn);
    fButtonsLayout->addWidget(fImageIndexSpin);
    fButtonsLayout->addWidget(fImageRotationSpin);
    // Add some space between the slice spin box and the curve tools (color, checkboxes, ...)
    fButtonsLayout->addSpacerItem(new QSpacerItem(1, 0, QSizePolicy::Expanding, QSizePolicy::Fixed));

    connect(fZoomInBtn, SIGNAL(clicked()), this, SLOT(OnZoomInClicked()));
    connect(fZoomOutBtn, SIGNAL(clicked()), this, SLOT(OnZoomOutClicked()));
    connect(fResetBtn, SIGNAL(clicked()), this, SLOT(OnResetClicked()));
    connect(fNextBtn, SIGNAL(clicked()), this, SLOT(OnNextClicked()));
    connect(fPrevBtn, SIGNAL(clicked()), this, SLOT(OnPrevClicked()));

    QSettings settings("VC.ini", QSettings::IniFormat);
    fCenterOnZoomEnabled = settings.value("viewer/center_on_zoom", false).toInt() != 0;
    fScrollSpeed = settings.value("viewer/scroll_speed", false).toInt();
    fSkipImageFormatConv = settings.value("perf/chkSkipImageFormatConvExp", false).toBool();

    QVBoxLayout* aWidgetLayout = new QVBoxLayout;
    aWidgetLayout->addWidget(fGraphicsView);
    aWidgetLayout->addLayout(fButtonsLayout);

    setLayout(aWidgetLayout);

    timerOverlayUpdate = new QTimer(this);
    timerOverlayUpdate->setSingleShot(true);
    connect(timerOverlayUpdate, &QTimer::timeout, this, &CVolumeViewer::UpdateOverlay);
    connect(this->GetView()->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        ScheduleOverlayUpdate();
    });
    connect(this->GetView()->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        ScheduleOverlayUpdate();
    });

    UpdateButtons();
}

// Destructor
CVolumeViewer::~CVolumeViewer(void)
{
    deleteNULL(fGraphicsView);
    deleteNULL(fScene);
    deleteNULL(fScrollArea);
    deleteNULL(fZoomInBtn);
    deleteNULL(fZoomOutBtn);
    deleteNULL(fResetBtn);
    deleteNULL(fPrevBtn);
    deleteNULL(fNextBtn);
    deleteNULL(fImageIndexSpin);
    deleteNULL(fImageRotationSpin);
}

void CVolumeViewer::SetButtonsEnabled(bool state)
{
    fZoomOutBtn->setEnabled(state);
    fZoomInBtn->setEnabled(state);
    fPrevBtn->setEnabled(state);
    fNextBtn->setEnabled(state);
    fImageIndexSpin->setEnabled(state);
    fImageRotationSpin->setEnabled(state);
}

void CVolumeViewer::SetImage(const QImage& nSrc)
{
    if (fImgQImage == nullptr) {
        fImgQImage = new QImage(nSrc);
    } else {
        *fImgQImage = nSrc;
    }

    // Create a QPixmap from the QImage
    QPixmap pixmap = QPixmap::fromImage(*fImgQImage, fSkipImageFormatConv ? Qt::NoFormatConversion : Qt::AutoColor);

    // Add the QPixmap to the scene as a QGraphicsPixmapItem
    if (!fBaseImageItem) {
        fBaseImageItem = fScene->addPixmap(pixmap);
    } else {
        fBaseImageItem->setPixmap(pixmap);
    }

    UpdateButtons();
    update();
}

void CVolumeViewer::SetImageIndex(int nImageIndex)
{
    fImageIndex = nImageIndex;
    fImageIndexSpin->setValue(nImageIndex);
    UpdateOverlay();
}

void CVolumeViewer::SetNumSlices(int num)
{
    fImageIndexSpin->setMaximum(num);
}

void CVolumeViewer::SetOverlaySettings(COverlayHandler::OverlaySettings overlaySettings)
{
    fOverlayHandler->setOverlaySettings(overlaySettings);
}

bool CVolumeViewer::eventFilter(QObject* watched, QEvent* event)
{
    // Wheel events
    if (watched == fGraphicsView || (fGraphicsView && watched == fGraphicsView->viewport()) && event->type() == QEvent::Wheel) {

        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);

        // Range key pressed
        if (fGraphicsView->isRangeKeyPressed()) {
            int numDegrees = wheelEvent->angleDelta().y() / 8;

            if (numDegrees > 0) {
                SendSignalImpactRangeUp();
            } else if (numDegrees < 0) {
                SendSignalImpactRangeDown();
            }

            return true;
        }
        // Ctrl = Zoom in/out
        else if (QApplication::keyboardModifiers() == Qt::ControlModifier) {
            int numDegrees = wheelEvent->angleDelta().y() / 8;

            if (numDegrees > 0) {
                OnZoomInClicked();
            } else if (numDegrees < 0) {
                OnZoomOutClicked();
                ScheduleOverlayUpdate();
            }

            if (fCenterOnZoomEnabled) {
                CenterOn(fGraphicsView->mapToScene(wheelEvent->position().toPoint()));
            }

            return true;
        }
        // Shift = Scan through slices
        else if (QApplication::keyboardModifiers() == Qt::ShiftModifier) {
            int numDegrees = wheelEvent->angleDelta().y() / 8;

            if (numDegrees > 0) {
                SendSignalOnNextSliceShift(fScanRange);
                fGraphicsView->showCurrentSliceIndex(fImageIndex, (GetViewState() == EViewState::ViewStateEdit && fImageIndex == sliceIndexToolStart));
            } else if (numDegrees < 0) {
                SendSignalOnPrevSliceShift(fScanRange);
                fGraphicsView->showCurrentSliceIndex(fImageIndex, (GetViewState() == EViewState::ViewStateEdit && fImageIndex == sliceIndexToolStart));
            }
            return true;
        }
        // Rotate key pressed
        else if (fGraphicsView->isRotateKyPressed()) {
            int delta = wheelEvent->angleDelta().y() / 22;
            fGraphicsView->rotate(delta);
            currentRotation += delta;
            currentRotation = currentRotation % 360;
            fImageRotationSpin->setValue(currentRotation);
            return true;
        }
        // View scrolling
        else {
            // If there is no valid scroll speed override value set, we rely
            // on the default handling of Qt, so we pass on the event.
            if (fScrollSpeed > 0) {
                // We have to add the two values since when pressing AltGr as the modifier,
                // the X component seems to be set by Qt
                int delta = wheelEvent->angleDelta().x() + wheelEvent->angleDelta().y();
                if (delta == 0) {
                    return true;
                }

                // Taken from QGraphicsView Qt source logic
                const bool horizontal = qAbs(wheelEvent->angleDelta().x()) > qAbs(wheelEvent->angleDelta().y());
                if (QApplication::keyboardModifiers() == Qt::AltModifier || horizontal) {
                    fGraphicsView->horizontalScrollBar()->setValue(
                        fGraphicsView->horizontalScrollBar()->value() + fScrollSpeed * ((delta < 0) ? 1 : -1));
                } else {
                    fGraphicsView->verticalScrollBar()->setValue(
                        fGraphicsView->verticalScrollBar()->value() + fScrollSpeed * ((delta < 0) ? 1 : -1));
                }
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void CVolumeViewer::ScaleImage(double nFactor)
{
    fScaleFactor *= nFactor;
    fGraphicsView->scale(nFactor, nFactor);

    UpdateButtons();
}

void CVolumeViewer::CenterOn(const QPointF& point)
{
    fGraphicsView->centerOn(point);
}

void CVolumeViewer::SetRotation(int degrees)
{
    if (currentRotation != degrees) {
        auto delta = (currentRotation - degrees) * -1;
        fGraphicsView->rotate(delta);
        currentRotation += delta;
        currentRotation = currentRotation % 360;
        fImageRotationSpin->setValue(currentRotation);
    }
}

void CVolumeViewer::Rotate(int delta)
{
    SetRotation(currentRotation + delta);
}

void CVolumeViewer::ResetRotation()
{
    fGraphicsView->rotate(-currentRotation);
    currentRotation = 0;
    fImageRotationSpin->setValue(currentRotation);
}

// Handle zoom in click
void CVolumeViewer::OnZoomInClicked(void)
{
    if (fZoomInBtn->isEnabled()) {
        ScaleImage(ZOOM_FACTOR);
    }
}

// Handle zoom out click
void CVolumeViewer::OnZoomOutClicked(void)
{
    if (fZoomOutBtn->isEnabled()) {
        ScaleImage(1 / ZOOM_FACTOR);
    }
}

void CVolumeViewer::OnResetClicked(void)
{
    fGraphicsView->resetTransform();
    fScaleFactor = 1.0;
    currentRotation = 0;
    fImageRotationSpin->setValue(currentRotation);

    UpdateButtons();
}

// Handle next image click
void CVolumeViewer::OnNextClicked(void)
{
    // send signal to controller (MVC) in order to update the content
    if (fNextBtn->isEnabled()) {
        // If the signal sender is the button, we check for Shift modifier for bigger jumps
        if (sender() == fNextBtn)
            SendSignalOnNextSliceShift(qga::keyboardModifiers() == Qt::ShiftModifier ? 10 : 1);
        else
            SendSignalOnNextSliceShift(1);
    }
}

// Handle previous image click
void CVolumeViewer::OnPrevClicked(void)
{
    // send signal to controller (MVC) in order to update the content
    if (fPrevBtn->isEnabled()) {
        // If the signal sender is the button, we check for Shift modifier for bigger jumps
        if (sender() == fPrevBtn)
            SendSignalOnPrevSliceShift(qga::keyboardModifiers() == Qt::ShiftModifier ? 10 : 1);
        else
            SendSignalOnPrevSliceShift(1);
    }
}

// Handle image index change
void CVolumeViewer::OnImageIndexSpinChanged(void)
{
    // send signal to controller in order to update the content
    if (fImageIndex != fImageIndexSpin->value()) {
        SendSignalOnLoadAnyImage(fImageIndexSpin->value());
    }
    SendSignalOnLoadAnyImage(fImageIndexSpin->value());
}

// Handle image rotation change
void CVolumeViewer::OnImageRotationSpinChanged(void)
{
    SetRotation(fImageRotationSpin->value());
}

// Update the status of the buttons
void CVolumeViewer::UpdateButtons(void)
{
    fZoomInBtn->setEnabled(fImgQImage != nullptr && fScaleFactor < 10.0);
    fZoomOutBtn->setEnabled(fImgQImage != nullptr && fScaleFactor > 0.05);
    fResetBtn->setEnabled(fImgQImage != nullptr && fabs(fScaleFactor - 1.0) > 1e-6);
    fNextBtn->setEnabled(fImgQImage != nullptr);
    fPrevBtn->setEnabled(fImgQImage != nullptr);
}

void CVolumeViewer::ScheduleOverlayUpdate()
{
    timerOverlayUpdate->start(20);
}

void CVolumeViewer::UpdateOverlay()
{ 
    // if (isPanning()) {
    //     return;
    // }
    for (auto overlay : overlayItems) {
        fScene->removeItem(overlay);
        delete overlay;
    }
    overlayItems.clear();

    const int alpha = 120;
    fOverlayHandler->updateOverlayData();    

    auto polygon = fGraphicsView->mapToScene(fGraphicsView->viewport()->rect());
    auto sceneRect = QRect(polygon.at(0).x(), polygon.at(0).y(), polygon.at(2).x() - polygon.at(0).x(), polygon.at(2).y() - polygon.at(0).y());

    // int x = static_cast<int>(std::floor(polygon.at(0).x()));
    // int y = static_cast<int>(std::floor(polygon.at(0).y()));
    // int w = std::ceil(polygon.at(2).x() - x);
    // int h = std::ceil(polygon.at(2).y() - y);
    
    // // Load a bit more around the actually needed rect, so we prevent/minimize white bars when panning
    // auto marginPercent = 0.2;
    // x -= w * marginPercent;
    // y -= h * marginPercent;
    // w += 2 * w * marginPercent; // 2 times, because we also moved x
    // h += 2 * h * marginPercent; // 2 times, because we also moved y
    
    // QRect sceneRect(std::max(0, x), std::max(0, y), w, h);

    // auto overlay1 = new COverlayGraphicsItem(fGraphicsView, this);
    // overlay1->setPen(QPen(QColor(80, 100, 210)));
    // overlay1->setBrush(QBrush(QColor(100, 120, 230, alpha)));
    // overlay1->setPoints(data[fImageIndex - 2]);
    // fScene->addItem(overlay1);
    // overlayItems.append(overlay1);

    auto dataM1 = fOverlayHandler->getOverlayDataForView(fImageIndex - 1);
    if (dataM1.size() > 0) {
        auto overlay2 = new COverlayGraphicsItem(fGraphicsView, dataM1, sceneRect, this);
        overlay2->setPen(QPen(QColor(140, 100, 160)));
        overlay2->setBrush(QBrush(QColor(160, 120, 180, alpha)));
        overlay2->setPos(sceneRect.left(), sceneRect.top());
        fScene->addItem(overlay2);
        overlayItems.append(overlay2);
    }

    auto data = fOverlayHandler->getOverlayDataForView(fImageIndex);
    if (data.size() > 0) {
        auto overlay3 = new COverlayGraphicsItem(fGraphicsView, data, sceneRect, this);
        overlay3->setPen(QPen(QColor(180, 90, 120)));
        overlay3->setBrush(QBrush(QColor(200, 110, 140, alpha)));
        overlay3->setPos(sceneRect.left(), sceneRect.top());
        fScene->addItem(overlay3);
        overlayItems.append(overlay3);
    }

    auto dataP1 = fOverlayHandler->getOverlayDataForView(fImageIndex + 1);
    if (dataP1.size() > 0) {
        auto overlay4 = new COverlayGraphicsItem(fGraphicsView, dataP1, sceneRect, this);
        overlay4->setPen(QPen(QColor(230, 100, 80)));
        overlay4->setBrush(QBrush(QColor(250, 120, 100, alpha)));
        overlay4->setPos(sceneRect.left(), sceneRect.top());
        fScene->addItem(overlay4);
        overlayItems.append(overlay4);
    }

    // auto overlay5 = new COverlayGraphicsItem(fGraphicsView, this);
    // overlay5->setPen(QPen(QColor(220, 140, 10)));
    // overlay5->setBrush(QBrush(QColor(255, 170, 30, alpha)));
    // overlay5->setPoints(data[fImageIndex + 2]);
    // fScene->addItem(overlay5);
    // overlayItems.append(overlay5);
}

// Reset the viewer
void CVolumeViewer::Reset()
{
    if (fBaseImageItem) {
        delete fBaseImageItem;
        fBaseImageItem = nullptr;
    }

    OnResetClicked(); // to reset zoom
}