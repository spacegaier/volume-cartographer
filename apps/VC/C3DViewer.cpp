// C3DViewer.cpp
// Philip Allgaier 2023 November
#include "C3DViewer.hpp"

#include <iostream>

#include <QtWidgets/QLayout>
#include <QtGui/QScreen>
#include <QtGui/QPainter>
#include <QtCore/QPropertyAnimation>

#include <Qt3DRender/QCameraLens>
#include <Qt3DCore/QTransform>

#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QDiffuseSpecularMapMaterial>
#include <Qt3DExtras/QTextureMaterial>

#include <Qt3DExtras/qtorusmesh.h>
#include <Qt3DRender/qmaterial.h>
#include <Qt3DRender/qtexture.h>
#include <Qt3DRender/qpointlight.h>
#include <Qt3DRender/QRenderStateSet>
#include <Qt3DRender/QCullFace>
#include <Qt3DRender/QRenderSurfaceSelector>
#include <Qt3DRender/QCameraSelector>
#include <Qt3DRender/QDepthTest>
#include <Qt3DRender/QViewport>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "UDataManipulateUtils.hpp"

using namespace ChaoVis;

TifTextureImage::TifTextureImage(QImage tif) {
    image = tif;
}

void TifTextureImage::paint(QPainter* painter) {
    auto rect = QRect(0, 0, image.width(), image.height());
    painter->drawImage(rect, image);
}

// Constructor
C3DViewer::C3DViewer(QWidget* parent)
: QFrame(parent)
{
    rootEntity = new Qt3DCore::QEntity;

    Qt3DExtras::Qt3DWindow *view = new Qt3DExtras::Qt3DWindow();
    view->defaultFrameGraph()->setClearColor(QColor(QRgb(0x4d4d4f)));
    QWidget* container = QWidget::createWindowContainer(view, this);
    QSize screenSize = view->screen()->size();
    container->setMinimumSize(QSize(500, 500));
    container->setMaximumSize(screenSize);

    // Camera
    camera = view->camera();
    camera->lens()->setPerspectiveProjection(45.0f, 16.0f/9.0f, 0.1f, 10000.0f);
    camera->setPosition(QVector3D(500, 0, 20.0f));
    camera->setUpVector(QVector3D(0, 1, 0));
    camera->setViewCenter(QVector3D(0, 0, 0));

    Qt3DCore::QEntity* lightEntity = new Qt3DCore::QEntity(camera);
    Qt3DRender::QPointLight* light = new Qt3DRender::QPointLight(lightEntity);
    light->setColor("white");
    light->setIntensity(1);
    lightEntity->addComponent(light);
    Qt3DCore::QTransform* lightTransform = new Qt3DCore::QTransform(lightEntity);
    lightTransform->setTranslation(camera->position());
    lightEntity->addComponent(lightTransform);

    // For camera controls
    camController = new Qt3DExtras::QOrbitCameraController(rootEntity);
    camController->setCamera(camera);
    camController->setLookSpeed(100);
    camController->setLinearSpeed(100);

    // Frame graph
    Qt3DRender::QRenderSurfaceSelector *surfaceSelector = new Qt3DRender::QRenderSurfaceSelector;
    surfaceSelector->setSurface(this);
    Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport(surfaceSelector);
    viewport->setNormalizedRect(QRectF(0, 0, 1.0, 1.0));
    Qt3DRender::QCameraSelector *cameraSelector = new Qt3DRender::QCameraSelector(viewport);
    cameraSelector->setCamera(camera);
    Qt3DRender::QClearBuffers *clearBuffers = new Qt3DRender::QClearBuffers(cameraSelector);
    clearBuffers->setBuffers(Qt3DRender::QClearBuffers::ColorDepthBuffer);
    clearBuffers->setClearColor(Qt::gray);
    Qt3DRender::QRenderStateSet *renderStateSet = new Qt3DRender::QRenderStateSet(clearBuffers);
    Qt3DRender::QCullFace *cullFace = new Qt3DRender::QCullFace(renderStateSet);
    cullFace->setMode(Qt3DRender::QCullFace::NoCulling);
    renderStateSet->addRenderState(cullFace);
    Qt3DRender::QDepthTest *depthTest = new Qt3DRender::QDepthTest;
    depthTest->setDepthFunction(Qt3DRender::QDepthTest::Less);
    renderStateSet->addRenderState(depthTest);
    view->setActiveFrameGraph(surfaceSelector);

    view->setRootEntity(rootEntity);
    setLayout(new QVBoxLayout);
    layout()->addWidget(container);
}

void C3DViewer::renderSegment(std::string segmentID) {
    if (segmentEntity) {
        delete segmentEntity;
    }
    segmentEntity = new Qt3DCore::QEntity(rootEntity);

    segmentMesh = new Qt3DRender::QMesh(segmentEntity);
    segmentMesh->setSource(QUrl::fromLocalFile(QString("/home/philip/dl.ash2txt.org/full-scrolls/Scroll1.volpkg/paths/%1/%1.obj").arg(QString::fromStdString(segmentID))));

    cv::Mat imgMat = cv::imread(QString("/home/philip/dl.ash2txt.org/full-scrolls/Scroll1.volpkg/paths/%1/%1.tif").arg(QString::fromStdString(segmentID)).toStdString(), -1);
    auto image = Mat2QImage(imgMat);
    auto tifTextureImage = new TifTextureImage(image);
    tifTextureImage->setWidth(image.width());
    tifTextureImage->setHeight(image.height());

    auto segmentMaterial = new Qt3DExtras::QTextureMaterial(segmentEntity);
    segmentMaterial->texture()->addTextureImage(tifTextureImage);

    auto meshTransform = new Qt3DCore::QTransform(segmentEntity);
    meshTransform->setScale3D(QVector3D(1, 1, 1));
    meshTransform->setTranslation(QVector3D(0, 0, 0));

    segmentEntity->addComponent(segmentMesh);
    segmentEntity->addComponent(meshTransform);
    Qt3DRender::QMaterial* phong = new Qt3DExtras::QPhongMaterial(rootEntity);
    segmentEntity->addComponent(phong);

    QObject::connect(segmentMesh, &Qt3DRender::QMesh::statusChanged, [this](const Qt3DRender::QMesh::Status status) {
        if(status == Qt3DRender::QMesh::Ready) {
            QObject::connect(this->segmentMesh->geometry(), &Qt3DCore::QGeometry::maxExtentChanged, [this](const QVector3D& maxExtent) {
                // std::cout << this->segmentMesh->geometry()->minExtent().x() << std::endl;
                // std::cout << this->segmentMesh->geometry()->minExtent().y() << std::endl;
                // std::cout << this->segmentMesh->geometry()->maxExtent().x() << std::endl;
                // std::cout << this->segmentMesh->geometry()->maxExtent().y() << std::endl;
                Qt3DCore::QTransform* transform = new Qt3DCore::QTransform();
                transform->setTranslation(QVector3D(1.5 * -this->segmentMesh->geometry()->minExtent().x(), 1.5 * -this->segmentMesh->geometry()->minExtent().y(), 0.0f));
                this->segmentEntity->addComponent(transform);

                this->camera->setPosition(QVector3D(
                    (this->segmentMesh->geometry()->maxExtent().x() - this->segmentMesh->geometry()->minExtent().x()) / 2,
                    (this->segmentMesh->geometry()->maxExtent().y() - this->segmentMesh->geometry()->minExtent().y()) / 2,
                    this->segmentMesh->geometry()->maxExtent().x() - this->segmentMesh->geometry()->minExtent().x()));
                    this->camera->viewAll();
            });
        }
    });
}