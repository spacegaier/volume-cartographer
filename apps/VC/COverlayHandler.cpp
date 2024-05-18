// COverlayHandler.cpp
// Philip Allgaier 2024 May
#include "COverlayHandler.hpp"

#include "CVolumeViewer.hpp"
#include "vc/core/io/OBJReader.hpp"
#include "vc/core/io/PLYReader.hpp"
#include "vc/core/types/ITKMesh.hpp"

using namespace ChaoVis;
namespace vc = volcart;
namespace fs = std::filesystem;

COverlayHandler::COverlayHandler(CVolumeViewer* volumeViewer) : viewer(volumeViewer)
{}

auto roundDownToNearestMultiple(float numToRound, int multiple)  ->int
{
    return ((static_cast<int>(numToRound) / multiple) * multiple);
}

auto COverlayHandler::determineOverlayFiles() -> QStringList
{
    if (settings.path.isEmpty()) {
        return QStringList();
    }

    std::set<QString> folderList;
    // Get the currently displayed region
    auto rect = viewer->GetView()->mapToScene(viewer->GetView()->viewport()->rect());
    std::cout << "Poly: " << rect.at(0).x() << "|" << rect.at(0).y() << std::endl;
    std::cout << "Poly: " << rect.at(1).x() << "|" << rect.at(1).y() << std::endl;
    std::cout << "Poly: " << rect.at(2).x() << "|" << rect.at(2).y() << std::endl;
    std::cout << "Poly: " << rect.at(3).x() << "|" << rect.at(3).y() << std::endl;

    auto xIndexStart = std::max(100, roundDownToNearestMultiple((rect.first().x() - 100) / settings.scale, settings.chunkSize) - settings.offset);
    xIndexStart -= settings.chunkSize; // due to the fact that file 000100 contains from -100 to 100, 000125 contains from 0 to 200, 000150 from 100 to 300
    auto yIndexStart = std::max(100, roundDownToNearestMultiple((rect.first().y() - 100) / settings.scale, settings.chunkSize) - settings.offset);
    yIndexStart -= settings.chunkSize;

    auto imageIndex = viewer->GetImageIndex();
    auto zIndexEnd = std::max(100, roundDownToNearestMultiple((imageIndex - 100) / settings.scale, settings.chunkSize) - settings.offset);
    auto zIndexStart = zIndexEnd - settings.chunkSize;

    auto xIndexEnd = roundDownToNearestMultiple((rect.at(2).x() - 100) / settings.scale, settings.chunkSize) - settings.offset;
    auto yIndexEnd = roundDownToNearestMultiple((rect.at(2).y() - 100) / settings.scale, settings.chunkSize) - settings.offset;

    QString folder;
    for (auto z = zIndexStart; z <= zIndexEnd; z += settings.chunkSize) {
        for (auto x = xIndexStart; x <= xIndexEnd; x += settings.chunkSize) {
            for (auto y = yIndexStart; y <= yIndexEnd; y += settings.chunkSize) {
                folder = QStringLiteral("%1")
                             .arg(y, 6, 10, QLatin1Char('0'))
                             .append("_" + QStringLiteral("%1").arg(z, 6, 10, QLatin1Char('0')))
                             .append("_" + QStringLiteral("%1").arg(x, 6, 10, QLatin1Char('0')));
                folderList.insert(folder);
            }
        }
    }

    QDir overlayMainFolder(settings.path);
    QStringList overlayFolders = overlayMainFolder.entryList(QDir::NoDotAndDotDot | QDir::AllEntries);
    // QStringList folderList = overlayFolders.filter(filename);

    QStringList fileList;
    for (auto folder : folderList) {
        QDir overlayFolder(overlayMainFolder.absolutePath() + QDir::separator() + folder);
        QStringList files = overlayFolder.entryList(QDir::NoDotAndDotDot | QDir::Files);

        for (auto file : files) {
            file = overlayFolder.path() + QDir::separator() + file;
            if (file.endsWith(".ply") || file.endsWith(".obj")) {
                fileList.append(file);
            }
        }
    }
    return fileList;
}

void COverlayHandler::setOverlaySettings(OverlaySettings overlaySettings)
{ 
    settings = overlaySettings; 

    // Hardf-code for testing
    settings.offset = -125;
    settings.xAxis = 2;
    settings.yAxis = 0;
    settings.zAxis = 1;
    settings.scale = 4;
    settings.chunkSize = 25;
    
    updateOverlayData();
}

void COverlayHandler::loadOverlayData(QStringList files)
{
    if (files.isEmpty()) {
        return;
    }

    vc::ITKMesh::Pointer mesh;
    itk::Point<double, 3> point;
    data.clear();

    for (auto file : files) {
        std::cout << "File: " << file.toStdString() << std::endl;
        if (file.endsWith(".ply")) {
            volcart::io::PLYReader reader(fs::path(file.toStdString()));
            reader.read();
            mesh = reader.getMesh();
        } else if (file.endsWith(".obj")) {
            volcart::io::OBJReader reader;
            reader.setPath(file.toStdString());
            reader.read();
            mesh = reader.getMesh();
        } else {
            continue;
        }

        auto numPoints = mesh->GetNumberOfPoints();

        for (std::uint64_t pnt_id = 0; pnt_id < numPoints; pnt_id++) {
            point = mesh->GetPoint(pnt_id);
            point[0] += settings.offset;
            point[1] += settings.offset;
            point[2] += settings.offset;
            point[0] *= settings.scale;
            point[1] *= settings.scale;
            point[2] *= settings.scale;

            if (point[settings.xAxis] > 0 && point[settings.yAxis] > 0 && point[settings.zAxis] > 0) {
                data[point[settings.zAxis]].push_back({
                    point[settings.xAxis],
                    point[settings.yAxis],
                });
            }
        }
    }

    for (auto it = data.begin(); it != data.end(); ++it) {
        std::cout << it->first << ", ";
    }
    std::cout << std::endl;
    // viewer->UpdateView();
}

void COverlayHandler::updateOverlayData()
{
    loadOverlayData(determineOverlayFiles());
}