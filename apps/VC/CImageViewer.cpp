// CImageViewer.cpp
// Chao Du 2015 April
#include "CImageViewer.hpp"
#include "HBase.hpp"

using namespace ChaoVis;
using qga = QGuiApplication;

#define BGND_RECT_MARGIN 8
#define DEFAULT_TEXT_COLOR QColor(255, 255, 120)
#define ZOOM_FACTOR 1.15

// Constructor
CImageViewerView::CImageViewerView(QWidget* parent)
: QGraphicsView(parent)
{
    timerTextAboveCursor = new QTimer(this);
    connect(timerTextAboveCursor, &QTimer::timeout, this, &CImageViewerView::hideTextAboveCursor);
    timerTextAboveCursor->setSingleShot(true);
}

void CImageViewerView::setup()
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

void CImageViewerView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_W) {
        rangeKeyPressed = true;
        event->accept();
    } else if (event->key() == Qt::Key_R) {
        curvePanKeyPressed = true;
        event->accept();
    }
}

void CImageViewerView::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_W)  {
        rangeKeyPressed = false;
        event->accept();
    } else if (event->key() == Qt::Key_R) {
        curvePanKeyPressed = false;
        event->accept();
    }
}

void CImageViewerView::showTextAboveCursor(const QString& value, const QString& label, const QColor& color)
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

void CImageViewerView::hideTextAboveCursor()
{
    textAboveCursor->setVisible(false);
    backgroundBehindText->setVisible(false);
}

void CImageViewerView::showCurrentImpactRange(int range)
{
    showTextAboveCursor(QString::number(range), "", QColor(255, 120, 110)); // tr("Impact Range")
}

void CImageViewerView::showCurrentScanRange(int range)
{
    showTextAboveCursor(QString::number(range), "", QColor(160, 180, 255)); // tr("Scan Range")
}

void CImageViewerView::showCurrentImageIndex(int slice, bool highlight)
{
    showTextAboveCursor(QString::number(slice), "", (highlight ? QColor(255, 50, 20) : QColor(255, 220, 30))); // tr("Slice")
}

// Constructor
CImageViewer::CImageViewer(QWidget* parent)
    : QWidget(parent)
    , fScrollArea(nullptr)
    , fGraphicsView(nullptr)
    , fZoomInBtn(nullptr)
    , fZoomOutBtn(nullptr)
    , fResetBtn(nullptr)
    , fNextBtn(nullptr)
    , fPrevBtn(nullptr)
    , fImgQImage(nullptr)
    , fBaseImageItem(nullptr)
    , fScaleFactor(1.0)
    , fImageIndex(0)
    , fScanRange(1)
{
    // Buttons
    fZoomInBtn = new QPushButton(tr("Zoom In"), this);
    fZoomOutBtn = new QPushButton(tr("Zoom Out"), this);
    fResetBtn = new QPushButton(tr("Reset"), this);
    fNextBtn = new QPushButton(tr("Next Image"), this);
    fPrevBtn = new QPushButton(tr("Previous Image"), this);

    // Image index edit
    fImageIndexEdit = new QSpinBox(this);
    fImageIndexEdit->setMinimum(0);
    fImageIndexEdit->setEnabled(true);
    fImageIndexEdit->setMinimumWidth(100);
    connect(fImageIndexEdit, &QSpinBox::editingFinished, this, &CImageViewer::OnImageIndexEditTextChanged);

    // Create graphics view
    fGraphicsView = new CImageViewerView(this);
    fGraphicsView->setRenderHint(QPainter::Antialiasing);
    setFocusProxy(fGraphicsView);

    // Create graphics scene
    fScene = new QGraphicsScene(this);

    // Set the scene
    fGraphicsView->setScene(fScene);
    fGraphicsView->setup();

    fGraphicsView->viewport()->installEventFilter(this);

    fButtonsLayout = new QHBoxLayout;
    fButtonsLayout->addWidget(fZoomInBtn);
    fButtonsLayout->addWidget(fZoomOutBtn);
    fButtonsLayout->addWidget(fResetBtn);
    fButtonsLayout->addWidget(fPrevBtn);
    fButtonsLayout->addWidget(fNextBtn);
    fButtonsLayout->addWidget(fImageIndexEdit);
    // Add some space between the image spin box and the curve tools (color, checkboxes, ...)
    fSpacer = new QSpacerItem(1, 0, QSizePolicy::Expanding, QSizePolicy::Fixed);
    fButtonsLayout->addSpacerItem(fSpacer);

    connect(fZoomInBtn, &QPushButton::clicked, this, &CImageViewer::OnZoomInClicked);
    connect(fZoomOutBtn, &QPushButton::clicked, this, &CImageViewer::OnZoomOutClicked);
    connect(fResetBtn, &QPushButton::clicked, this, &CImageViewer::OnResetClicked);
    connect(fNextBtn, &QPushButton::clicked, this, &CImageViewer::OnNextClicked);
    connect(fPrevBtn, &QPushButton::clicked, this, &CImageViewer::OnPrevClicked);

    QSettings settings("VC.ini", QSettings::IniFormat);
    fCenterOnZoomEnabled = settings.value("viewer/center_on_zoom", false).toInt() != 0;

    QVBoxLayout* aWidgetLayout = new QVBoxLayout;
    aWidgetLayout->addWidget(fGraphicsView);
    aWidgetLayout->addLayout(fButtonsLayout);

    setLayout(aWidgetLayout);

    UpdateButtons();

    timerChunkUpdate = new QTimer(this);
    timerChunkUpdate->setSingleShot(true);
    connect(timerChunkUpdate, &QTimer::timeout, this, [this]() { SendSignalOnLoadAnyImage(fImageIndex); });
    connect(this->GetView()->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        ScheduleChunkUpdate();
    });
    connect(this->GetView()->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        ScheduleChunkUpdate();
    });
}

// Destructor
CImageViewer::~CImageViewer(void)
{
    deleteNULL(fGraphicsView);
    deleteNULL(fScene);
    deleteNULL(fScrollArea);
    deleteNULL(fZoomInBtn);
    deleteNULL(fZoomOutBtn);
    deleteNULL(fResetBtn);
    deleteNULL(fPrevBtn);
    deleteNULL(fNextBtn);
    deleteNULL(fImageIndexEdit);
}

void CImageViewer::setButtonsEnabled(bool state)
{
    fZoomOutBtn->setEnabled(state);
    fZoomInBtn->setEnabled(state);
    fPrevBtn->setEnabled(state);
    fNextBtn->setEnabled(state);
    fImageIndexEdit->setEnabled(state);
}

void CImageViewer::SetImage(const QImage& nSrc, const QPoint pos)
{
    if (fImgQImage == nullptr) {
        fImgQImage = new QImage(nSrc);
    } else {
        *fImgQImage = nSrc;
    }

    auto rect = GetView()->mapToScene(GetView()->viewport()->rect());

    // Create a QPixmap from the QImage
    QPixmap pixmap = QPixmap::fromImage(*fImgQImage, Qt::NoFormatConversion);

    if (!fBaseImageItem) {
        fBaseImageItem = fScene->addPixmap(pixmap);
    } else {
        fBaseImageItem->setPixmap(pixmap);
    }

    QPoint newPos = {std::max(0, pos.x()), std::max(0, pos.y())};
    fBaseImageItem->setPos(newPos);
    //fGraphicsView->centerOn(fBaseImageItem);

    UpdateButtons();
    update();
}

void CImageViewer::SetNumImages(int num)
{
    fImageIndexEdit->setMaximum(num);
}

// Set the scan range
void CImageViewer::SetScanRange(int scanRange)
{
    fScanRange = scanRange;
    fGraphicsView->showCurrentScanRange(scanRange);
}

bool CImageViewer::eventFilter(QObject* watched, QEvent* event)
{
    // check for mouse release generic
    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        mouseReleaseEvent(mouseEvent);
        event->accept();
        return true;
    }

    if (event->type() == QEvent::MouseMove) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);

        // Transform the global coordinates to local coordinates
        QPointF localPoint = this->mapFromGlobal(mouseEvent->globalPosition());

        // Create a new QMouseEvent with local coordinates
        QMouseEvent localMouseEvent(QEvent::MouseMove,
                                    localPoint,
                                    mouseEvent->globalPosition(),
                                    mouseEvent->button(),
                                    mouseEvent->buttons(),
                                    mouseEvent->modifiers());

        // Manually call your mouseMoveEvent function
        mouseMoveEvent(&localMouseEvent);

        event->accept();
        return true;
    }

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
                SendSignalOnNextImageShift(fScanRange);
                fGraphicsView->showCurrentImageIndex(fImageIndex, ShouldHighlightImageIndex(fImageIndex));
            } else if (numDegrees < 0) {
                SendSignalOnPrevImageShift(fScanRange);
                fGraphicsView->showCurrentImageIndex(fImageIndex, ShouldHighlightImageIndex(fImageIndex));
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void CImageViewer::ScaleImage(double nFactor)
{
    fScaleFactor *= nFactor;
    fGraphicsView->scale(nFactor, nFactor);

    UpdateButtons();
}

void CImageViewer::CenterOn(const QPointF& point)
{
    fGraphicsView->centerOn(point);
}

// Handle zoom in click
void CImageViewer::OnZoomInClicked(void)
{
    if (fZoomInBtn->isEnabled()) {
        ScaleImage(ZOOM_FACTOR);
    }
}

// Handle zoom out click
void CImageViewer::OnZoomOutClicked(void)
{
    if (fZoomOutBtn->isEnabled()) {
        ScaleImage(1 / ZOOM_FACTOR);
    }
}

void CImageViewer::OnResetClicked(void)
{
    fGraphicsView->resetTransform();
    fScaleFactor = 1.0;

    UpdateButtons();
}

// Handle next image click
void CImageViewer::OnNextClicked(void)
{
    // send signal to controller (MVC) in order to update the content
    if (fNextBtn->isEnabled()) {
        // If the signal sender is the button, we check for Shift modifier for bigger jumps
        if (sender() == fNextBtn)
            SendSignalOnNextImageShift(qga::keyboardModifiers() == Qt::ShiftModifier ? 10 : 1);
        else
            SendSignalOnNextImageShift(1);
    }
}

// Handle previous image click
void CImageViewer::OnPrevClicked(void)
{
    // send signal to controller (MVC) in order to update the content
    if (fPrevBtn->isEnabled()) {
        // If the signal sender is the button, we check for Shift modifier for bigger jumps
        if (sender() == fPrevBtn)
            SendSignalOnPrevImageShift(qga::keyboardModifiers() == Qt::ShiftModifier ? 10 : 1);
        else
            SendSignalOnPrevImageShift(1);
    }
}

// Handle image index change
void CImageViewer::OnImageIndexEditTextChanged(void)
{
    // send signal to controller in order to update the content
    if (fImageIndexEdit->value() != fImageIndex) {
        SendSignalOnLoadAnyImage(fImageIndexEdit->value());
    }
}

// Update the status of the buttons
void CImageViewer::UpdateButtons(void)
{
    fZoomInBtn->setEnabled(fImgQImage != nullptr && fScaleFactor < 10.0);
    fZoomOutBtn->setEnabled(fImgQImage != nullptr && fScaleFactor > 0.05);
    fResetBtn->setEnabled(
        fImgQImage != nullptr && fabs(fScaleFactor - 1.0) > 1e-6);
    fNextBtn->setEnabled(fImgQImage != nullptr);
    fPrevBtn->setEnabled(fImgQImage != nullptr);
}

// Adjust scroll bar of scroll area
void CImageViewer::AdjustScrollBar(QScrollBar* nScrollBar, double nFactor)
{
    nScrollBar->setValue(
        int(nFactor * nScrollBar->value() +
            ((nFactor - 1) * nScrollBar->pageStep() / 2)));
}

auto CImageViewer::CleanScrollPosition(cv::Vec2f pos) const -> cv::Vec2f 
{
    int x = pos[0];
    int y = pos[1];

    // Get the size of the QGraphicsView viewport
    int viewportWidth = fGraphicsView->viewport()->width();
    int viewportHeight = fGraphicsView->viewport()->height();

    // Calculate the position of the scroll bars
    int horizontalPos = x - viewportWidth / 2;
    int verticalPos = y - viewportHeight / 2;

    // Check and respect horizontal boundaries
    if (horizontalPos < fGraphicsView->horizontalScrollBar()->minimum())
        horizontalPos = fGraphicsView->horizontalScrollBar()->minimum();
    else if (horizontalPos > fGraphicsView->horizontalScrollBar()->maximum())
        horizontalPos = fGraphicsView->horizontalScrollBar()->maximum();

    // Check and respect vertical boundaries
    if (verticalPos < fGraphicsView->verticalScrollBar()->minimum())
        verticalPos = fGraphicsView->verticalScrollBar()->minimum();
    else if (verticalPos > fGraphicsView->verticalScrollBar()->maximum())
        verticalPos = fGraphicsView->verticalScrollBar()->maximum();

    return cv::Vec2f(horizontalPos + viewportWidth / 2, verticalPos + viewportHeight / 2);
}

void CImageViewer::ScrollToCenter(cv::Vec2f pos)
{
    pos = CleanScrollPosition(pos);

    // Get the size of the QGraphicsView viewport
    int viewportWidth = fGraphicsView->viewport()->width();
    int viewportHeight = fGraphicsView->viewport()->height();

    // Calculate the position of the scroll bars
    int horizontalPos = pos[0] - viewportWidth / 2;
    int verticalPos = pos[1] - viewportHeight / 2;

    // Set the scroll bar positions
    fGraphicsView->horizontalScrollBar()->setValue(horizontalPos);
    fGraphicsView->verticalScrollBar()->setValue(verticalPos);
}

auto CImageViewer::GetScrollPosition() const -> cv::Vec2f
{
    // Get the positions of the scroll bars
    float horizontalPos = static_cast<float>(fGraphicsView->horizontalScrollBar()->value() + fGraphicsView->viewport()->width() / 2);
    float verticalPos = static_cast<float>(fGraphicsView->verticalScrollBar()->value() + fGraphicsView->viewport()->height() / 2);

    // Return as cv::Vec2f
    return cv::Vec2f(horizontalPos, verticalPos);
}

void CImageViewer::ResetView()
{
    // Remove all existing items
    QList<QGraphicsItem*> allItems = fScene->items();
    for (QGraphicsItem* item : allItems) {
        fScene->removeItem(item);
        delete item;
    }
}

// Handle mouse press event
void CImageViewer::mousePressEvent(QMouseEvent* event)
{
    // Return if not left or right click
    if (!(event->buttons() & Qt::RightButton) && !(event->buttons() & Qt::LeftButton)) {
        return;
    }

    if (event->button() == Qt::RightButton) {
        // Attempt to start the panning. We only consider us in panning mode, if in the mouse move event
        // we actually detect movement.

        rightPressed = true;
        wantsPanning = true;
        isPanning = false;
        panStartX = event->position().x();
        panStartY = event->position().y();
    }
}

// Handle mouse move event
void CImageViewer::mouseMoveEvent(QMouseEvent* event)
{
    if (!wantsPanning && !rightPressed) {
        return;
    }

    if (wantsPanning && rightPressed){
        // We potentially want to start panning, and now check if the mouse actually moved
        if (event->position().x() != panStartX || event->position().y() - panStartY)
        {
            isPanning = true;
            setCursor(Qt::ClosedHandCursor);
            fGraphicsView->horizontalScrollBar()->setValue(fGraphicsView->horizontalScrollBar()->value() - 2 * (event->position().x() - panStartX));
            fGraphicsView->verticalScrollBar()->setValue(fGraphicsView->verticalScrollBar()->value() - 2 * (event->position().y() - panStartY));
            panStartX = event->position().x();
            panStartY = event->position().y();
        } else {
            wantsPanning = false;
        }
    }
}

// Handle mouse release event
void CImageViewer::mouseReleaseEvent(QMouseEvent* event)
{
    // end panning
    if (event->button() == Qt::RightButton) {
        isPanning = wantsPanning = rightPressed = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    setCursor(Qt::ArrowCursor);
}

void CImageViewer::ScheduleChunkUpdate()
{
    timerChunkUpdate->start(50);
}