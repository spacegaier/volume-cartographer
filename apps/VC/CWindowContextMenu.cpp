// CWindowContextMenu.cpp
// Philip Allgaier 2023 November
#include "CWindow.hpp"

#include "CLayerViewer.hpp"
#include "C3DViewer.hpp"
#include "UDataManipulateUtils.hpp"

#include "vc/core/io/ImageIO.hpp"
#include "vc/core/io/MeshIO.hpp"
#include "vc/core/util/MeshMath.hpp"
#include "vc/core/util/String.hpp"
#include "vc/core/util/Iteration.hpp"
#include "vc/core/util/Logging.hpp"
#include "vc/core/types/UVMap.hpp"
#include "vc/meshing/ACVD.hpp"
#include "vc/meshing/ITK2VTK.hpp"
#include "vc/meshing/OrderedPointSetMesher.hpp"
#include "vc/texturing/AngleBasedFlattening.hpp"
#include "vc/texturing/LayerTexture.hpp"
#include "vc/texturing/PPMGenerator.hpp"

#include <QFutureWatcher>
#include <QtConcurrent>

namespace vc = volcart;
namespace fs = volcart::filesystem;

using namespace ChaoVis;

void CWindow::OnPathCustomContextMenu(const QPoint& point)
{
    QModelIndex index = fPathListWidget->indexAt(point);
    if (index.isValid()) {
        std::string segID = fPathListWidget->itemFromIndex(index)->text(0).toStdString();

        QAction* actGenLayers = new QAction(tr("Generate Layers"), this);
        connect(actGenLayers, &QAction::triggered, this, [segID, this](){ OnPathGenerateLayers(segID); });

        if (!fSegStructMap[segID].display && !fSegStructMap[segID].compute) {
            actGenLayers->setDisabled(true);
        }

        QAction* actRender3D = new QAction(tr("Render 3D"), this);
        connect(actRender3D, &QAction::triggered, this, [segID, this](){ OnPathRender3D(segID); });

        if (!fSegStructMap[segID].display && !fSegStructMap[segID].compute) {
            actRender3D->setDisabled(true);
        }

        QMenu menu(this);
        menu.addAction(actGenLayers);
        menu.addAction(actRender3D);

        menu.exec(fPathListWidget->viewport()->mapToGlobal(point));
    }
}

static constexpr double SAMPLING_DENSITY_FACTOR = 50;
// Square Micron to square millimeter conversion factor
static constexpr double UM_TO_MM = 0.001 * 0.001;
// Min. number of points required to do flattening
static constexpr size_t CLEANER_MIN_REQ_POINTS = 100;

void CWindow::GenerateLayers(QPromise<void> &promise) {

    promise.setProgressRange(0, 100);
    promise.setProgressValue(0);

    auto outputPath = fs::canonical(fLayerViewerWidget->getTempPath().toStdString());
    if (fs::exists(outputPath)) {
        // Clear old content
        for (const auto& entry : std::filesystem::directory_iterator(outputPath)) {
            std::filesystem::remove_all(entry.path());
        }
    }

    ///// Resample the segmentation /////
    // Mesh the point cloud
    vc::meshing::OrderedPointSetMesher mesher;
    mesher.setPointSet(fSegStructMap[fSegIdLayers].fMasterCloud);
    auto input = mesher.compute();

    // Calculate sampling density
    auto voxelToMicron = std::pow(currentVolume->voxelSize(), 2);
    auto area = vc::meshmath::SurfaceArea(input) * voxelToMicron * UM_TO_MM;
    auto vertCount = static_cast<size_t>(SAMPLING_DENSITY_FACTOR * area);
    vertCount = (vertCount < CLEANER_MIN_REQ_POINTS) ? CLEANER_MIN_REQ_POINTS
                                                     : vertCount;

    // Decimate using ACVD
    if (promise.isCanceled()) {
        return;
    }
    std::cout << "Resampling mesh..." << std::endl;
    promise.setProgressValueAndText(10, tr("Resampling mesh... %p%"));
    vc::meshing::ACVD resampler;
    resampler.setInputMesh(input);
    resampler.setNumberOfClusters(vertCount);
    auto itkACVD = resampler.compute();

    ///// ABF flattening /////
    if (promise.isCanceled()) {
        return;
    }
    std::cout << "Computing parameterization..." << std::endl;
    promise.setProgressValueAndText(20, tr("Parameterization... %p%"));
    vc::texturing::AngleBasedFlattening abf;
    abf.setMesh(itkACVD);
    abf.compute();

    // Get UV map
    auto uvMap = abf.getUVMap();
    auto width = static_cast<size_t>(std::ceil(uvMap->ratio().width));
    auto height = static_cast<size_t>(std::ceil(uvMap->ratio().height));

    vc::WriteMesh(outputPath.native() + "/" + fSegIdLayers + ".obj", itkACVD, uvMap);

    vc::UVMap::AlignToAxis(*uvMap, itkACVD, vc::UVMap::AlignmentAxis::ZPos);

    auto outputPathLayers = fs::path(outputPath.native() + "/layers");
    if (!fs::exists(outputPathLayers)) {
        fs::create_directory(outputPathLayers);
    }
    auto outputPathMask = outputPath.native() + "/" + fSegIdLayers + "_mask.png";
    auto outputPathUV = outputPath.native() + "/" + fSegIdLayers + "_uv.png";
    auto outputPathPPM = outputPath.native() + "/" + fSegIdLayers + ".ppm";

    // Generate the PPM
    if (promise.isCanceled()) {
        return;
    }
    std::cout << "Generating PPM..." << std::endl;
    promise.setProgressValueAndText(30, tr("Generating PPM... %p%"));
    vc::texturing::PPMGenerator ppmGen;
    ppmGen.setMesh(itkACVD);
    ppmGen.setUVMap(uvMap);
    ppmGen.setDimensions(height, width);
    auto ppm = ppmGen.compute();
    vc::PerPixelMap::WritePPM(outputPathPPM, *ppm);

    double radius = fVpkg->materialThickness() / currentVolume->voxelSize();

    // Setup line generator
    auto line = vc::LineGenerator::New();
    line->setSamplingRadius(radius);
    line->setSamplingInterval(1);
    // line->setSamplingDirection(vc::Bidirectional);

    // Layer texture
    if (promise.isCanceled()) {
        return;
    }
    std::cout << "Generating layers..." << std::endl;
    promise.setProgressValueAndText(55, tr("Generating layers... %p%"));
    vc::texturing::LayerTexture s;
    s.setVolume(currentVolume);
    s.setPerPixelMap(ppm);
    s.setGenerator(line);

    auto texture = s.compute();

    auto imgFmt = "tif";
    vc::WriteImageOpts writeOpts;
    // Default for tiff in this app: No compression
    if (imgFmt == "tif" or imgFmt == "tiff") {
        writeOpts.compression = 1;
    }

    // Write the layers
    if (promise.isCanceled()) {
        return;
    }
    std::cout << "Writing layers..." << std::endl;
    promise.setProgressValueAndText(90, tr("Writing layers... %p%"));
    const int numChars =
        static_cast<int>(std::to_string(texture.size()).size());
    fs::path filepath;
    for (const auto [i, image] : vc::enumerate(texture)) {
        auto fileName = vc::to_padded_string(i, numChars) + "." + imgFmt;
        filepath = outputPathLayers / fileName;
        vc::WriteImage(filepath, image, writeOpts);
    }

    vc::WriteImage(outputPathUV, vc::UVMap::Plot(*uvMap, abf.getMesh()));

    fLayerViewerWidget->SetNumImages(texture.size());
    fLayerViewerWidget->setPPM(*ppm);
}

void CWindow::OnPathGenerateLayers(std::string segmentID) {

    if (segmentID.empty()) {
        return;
    }

    fSegIdLayers = segmentID;

    QObject::connect(&watcherLayers, &QFutureWatcher<void>::progressValueChanged, [this](int progress){
        this->fLayerViewerWidget->setProgress(progress);
    });
    QObject::connect(&watcherLayers, &QFutureWatcher<void>::progressTextChanged, [this](QString text){
        this->fLayerViewerWidget->setProgressText(text);
    });
    QObject::connect(&watcherLayers, &QFutureWatcher<void>::finished, [this](){
        this->OpenLayer();
        this->dockWidgetLayers->show();
        this->fLayerViewerWidget->setProgress(0);
        this->fLayerViewerWidget->setProgressText(tr("Done"));
    });
    watcherLayers.setFuture(QtConcurrent::run(&CWindow::GenerateLayers, this));
}

void CWindow::OnPathRender3D(std::string segmentID) {

    if (segmentID.empty()) {
        return;
    }

    fSegIdLayers = segmentID;

    QObject::connect(&watcherLayers, &QFutureWatcher<void>::progressValueChanged, [this, segmentID](int progress){
        if (progress == 30) {
            this->f3DViewerWidget->renderSegment(segmentID);
            this->dockWidget3D->show();
        }
    });
    watcherLayers.setFuture(QtConcurrent::run(&CWindow::GenerateLayers, this));
}