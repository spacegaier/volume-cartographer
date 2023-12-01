// C3DViewer.hpp
// Philip Allgaier 2023 November
#pragma once

#include <QtWidgets/QFrame>

#include <Qt3DCore/QEntity>
#include <Qt3DRender/QMesh>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QPaintedTextureImage>
#include <Qt3DExtras/QOrbitCameraController>

namespace ChaoVis
{

class TifTextureImage : public Qt3DRender::QPaintedTextureImage
{
public:
    TifTextureImage(QImage tif);
    void paint(QPainter* painter);

protected:
    QImage image{};
};

class C3DViewer : public QFrame
{
    Q_OBJECT

    public:
        C3DViewer(QWidget* parent = nullptr);

        void renderSegment(std::string segmentID);

    protected:
        Qt3DCore::QEntity* rootEntity{nullptr};
        Qt3DRender::QCamera* camera{nullptr};
        Qt3DRender::QMesh* segmentMesh{nullptr};
        Qt3DCore::QEntity* segmentEntity{nullptr};
        Qt3DExtras::QOrbitCameraController* camController{nullptr};
};

}