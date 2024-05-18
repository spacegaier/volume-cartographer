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

    int numThreads = static_cast<int>(std::thread::hardware_concurrency()) - 2;
    for (int f = 0; f <= files.size(); f += numThreads) {
        std::vector<std::thread> threads;

        for (int i = 0; i <= numThreads && (f+i) < files.size(); i++) {
            threads.emplace_back(&COverlayHandler::loadSingleOverlayFile, this, files.at(f+i));
        }

        for (auto& t : threads) {
            t.join();
        }

        // // Check if prefetching was stopped or slice index changed
        // if (stopPrefetching.load() || prefetchSliceIndex.load() != currentSliceIndex) {
        //     break;
        // }
    }

    for (auto it = data.begin(); it != data.end(); ++it) {
        std::cout << it->first << ", ";
    }
    std::cout << std::endl;
}

void COverlayHandler::loadSingleOverlayFile(QString file) const
{
    vc::ITKMesh::Pointer mesh;
    itk::Point<double, 3> point;
    OverlayData threadData;

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
        return;
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
            threadData[point[settings.zAxis]].push_back({
                point[settings.xAxis],
                point[settings.yAxis],
            });
        }
    }

    if (threadData.size() > 0) {
        mergeThreadData(threadData);
    }
}

void COverlayHandler::mergeThreadData(OverlayData threadData) const
{
    std::lock_guard<std::shared_mutex> lock(dataMutex);

    for (auto it = threadData.begin(); it != threadData.end(); ++it) {
        std::pair<OverlayData::iterator, bool> ins = data.insert(*it);
        if (!ins.second) {  
            // Map key already existed, so we have to merge the slice data
            OverlaySliceData* vec1 = &(it->second);
            OverlaySliceData* vec2 = &(ins.first->second);
            vec2->insert(vec2->end(), vec1->begin(), vec1->end());
        }
    }
}

void COverlayHandler::updateOverlayData()
{
    loadOverlayData(determineOverlayFiles());
}