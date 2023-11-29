// CWindowContextMenu.cpp
// Philip Allgaier 2023 Nov
#include "CWindow.hpp"
#include <opencv2/imgproc.hpp>

#include "CXCurve.hpp"
#include "CLayerViewer.hpp"
#include "UDataManipulateUtils.hpp"

#include "vc/core/util/Iteration.hpp"
#include "vc/core/util/Logging.hpp"
#include "vc/meshing/OrderedPointSetMesher.hpp"

#include "vc/core/io/ImageIO.hpp"
#include "vc/core/util/MeshMath.hpp"
#include "vc/core/util/String.hpp"
#include "vc/core/types/UVMap.hpp"
#include "vc/meshing/ACVD.hpp"
#include "vc/meshing/ITK2VTK.hpp"
#include "vc/meshing/OrderedPointSetMesher.hpp"
#include "vc/meshing/SmoothNormals.hpp"
#include "vc/texturing/AngleBasedFlattening.hpp"
#include "vc/texturing/LayerTexture.hpp"
#include "vc/texturing/PPMGenerator.hpp"

#include <QFutureWatcher>
#include <QtConcurrent>

namespace vc = volcart;
namespace vcs = volcart::segmentation;
namespace fs = volcart::filesystem;

using namespace ChaoVis;

void CWindow::OnPathCustomContextMenu(const QPoint& point)
{
    QModelIndex index = fPathListWidget->indexAt(point);
    if (index.isValid()) {
        QAction* actVcRender = new QAction(tr("Run vc_render"), this);
        std::string segID = fPathListWidget->itemFromIndex(index)->text(0).toStdString();
        connect(actVcRender, &QAction::triggered, this, [segID, this](){ OnPathRunVcRender(segID); });

        QAction* actInkDetect = new QAction(tr("Run Layer Generation"), this);
        connect(actInkDetect, &QAction::triggered, this, [segID, this](){ OnPathRunInkDetection(segID); });

        QMenu menu(this);
        menu.addAction(actVcRender);
        menu.addAction(actInkDetect);

        menu.exec(fPathListWidget->viewport()->mapToGlobal(point));
    }
}

void CWindow::OnPathRunVcRender(std::string segmentID)
{
    auto folder = fVpkg->segmentation(segmentID)->path().native();
    // if (!fs::exists(folder)) {
    //     fs::create_directory(folder);
    // }
    // auto outputPath = fs::canonical(folder);

    QString program = "./vc_render";
    QStringList arguments;
    arguments << "-v" << fVpkgPath << "-s" << QString::fromStdString(segmentID) << "-o" << QString("%1/%2.obj").arg(QString::fromStdString(folder)).arg(QString::fromStdString(segmentID))
        << "--uv-plot" << QString("%1/uv_%2.tif").arg(QString::fromStdString(folder)).arg(QString::fromStdString(segmentID)) << "--mesh-resample-smoothing" << "3"
        << "--output-ppm" << QString("%1/%2.ppm").arg(QString::fromStdString(folder)).arg(QString::fromStdString(segmentID));

    //vc_render -v my-project.volpkg -s 20230503225234 -o test_20230503225234.obj

    std::cout << "Starting vc_render for segment " << segmentID << std::endl;

    //std::cout << QDir::tempPath().toStdString() << std::endl;

    std::cout << "Used arguments: ";
    for(auto arg : arguments) {
        std::cout << arg.toStdString() << " ";
    }
    std::cout << std::endl;

    QProcess *myProcess = new QProcess(this);
    QString name = "vc_render";
    connect(myProcess, &QProcess::finished, this, [this, segmentID, name](){
        std::cout << "Finished: " << segmentID << ": " << name.toStdString() << std::endl;
        auto vc_layers = new QProcess(this);
        QStringList arguments;
        arguments << "-p"  << QString("1.ppm").arg(QString::fromStdString(segmentID)) << "--output-dir" << "layers/";
        vc_layers->start("vc_layers_from_ppm", arguments);
    });
    connect(myProcess, &QProcess::errorOccurred, this, [this, segmentID, name](){ std::cout << "Error: " << segmentID << ": " << name.toStdString() << std::endl; });
    myProcess->start(program, arguments);
}

static constexpr double SAMPLING_DENSITY_FACTOR = 50;
// Square Micron to square millimeter conversion factor
static constexpr double UM_TO_MM = 0.001 * 0.001;
// Min. number of points required to do flattening
static constexpr size_t CLEANER_MIN_REQ_POINTS = 100;

void CWindow::GenerateLayers(QPromise<void> &promise) {

    promise.setProgressRange(0, 100);
    promise.setProgressValue(0);

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
    std::cout << "Resampling mesh..." << std::endl;
    promise.setProgressValueAndText(10, tr("Resampling mesh... %p%"));
    vc::meshing::ACVD resampler;
    resampler.setInputMesh(input);
    resampler.setNumberOfClusters(vertCount);
    auto itkACVD = resampler.compute();

    ///// ABF flattening /////
    std::cout << "Computing parameterization..." << std::endl;
    vc::texturing::AngleBasedFlattening abf;
    abf.setMesh(itkACVD);
    abf.compute();

    // Get UV map
    auto uvMap = abf.getUVMap();
    auto width = static_cast<size_t>(std::ceil(uvMap->ratio().width));
    auto height = static_cast<size_t>(std::ceil(uvMap->ratio().height));

    vc::UVMap::AlignToAxis(*uvMap, itkACVD, vc::UVMap::AlignmentAxis::ZPos);

    auto folder = fVpkg->segmentation(fSegIdLayers)->path().native() + "/layers";
    if (!fs::exists(folder)) {
        fs::create_directory(folder);
    }
    auto outputPath = fs::canonical(folder);
    auto outputPathMask = fVpkg->segmentation(fSegIdLayers)->path().native() + "/" + fSegIdLayers + "_mask.png";
    auto outputPathUV = fVpkg->segmentation(fSegIdLayers)->path().native() + "/" + fSegIdLayers + "_uv.png";
    auto outputPathPPM = fVpkg->segmentation(fSegIdLayers)->path().native() + "/" + fSegIdLayers + ".ppm";

    // Generate the PPM
    std::cout << "Generating PPM..." << std::endl;
    promise.setProgressValueAndText(25, tr("Generating PPM... %p%"));
    vc::texturing::PPMGenerator ppmGen;
    ppmGen.setMesh(itkACVD);
    ppmGen.setUVMap(uvMap);
    ppmGen.setDimensions(height, width);
    auto ppm = ppmGen.compute();
    vc::PerPixelMap::WritePPM(outputPathPPM, *ppm);

    promise.setProgressValue(25);

    double radius = fVpkg->materialThickness() / currentVolume->voxelSize();

    // Setup line generator
    auto line = vc::LineGenerator::New();
    line->setSamplingRadius(radius);
    line->setSamplingInterval(1);
    // line->setSamplingDirection(vc::Bidirectional);

    // Layer texture
    std::cout << "Generating layers..." << std::endl;
    promise.setProgressValueAndText(50, tr("Generating layers... %p%"));
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
    std::cout << "Writing layers..." << std::endl;
    promise.setProgressValueAndText(90, tr("Writing layers... %p%"));
    const int numChars =
        static_cast<int>(std::to_string(texture.size()).size());
    fs::path filepath;
    for (const auto [i, image] : vc::enumerate(texture)) {
        auto fileName = vc::to_padded_string(i, numChars) + "." + imgFmt;
        filepath = outputPath / fileName;
        vc::WriteImage(filepath, image, writeOpts);
    }

    vc::WriteImage(outputPathUV, vc::UVMap::Plot(*uvMap, abf.getMesh()));

    fLayerViewerWidget->SetNumImages(texture.size());
    fLayerViewerWidget->setPPM(*ppm);

    promise.setProgressValueAndText(100, tr("Done"));
}

void CWindow::OnPathRunInkDetection(std::string segmentID) {

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
    });
    watcherLayers.setFuture(QtConcurrent::run(&CWindow::GenerateLayers, this));
}