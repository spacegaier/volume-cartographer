// COverlayLoader.cpp
// Philip Allgaier 2024 May
#include "COverlayLoader.hpp"

#include "CVolumeViewer.hpp"
#include "vc/core/io/OBJReader.hpp"
#include "vc/core/io/PLYReader.hpp"
#include "vc/core/types/ITKMesh.hpp"

using namespace ChaoVis;
namespace vc = volcart;
namespace fs = std::filesystem;

COverlayLoader::COverlayLoader()
{}

auto roundDownToNearestMultiple(float numToRound, int multiple)  ->int
{
    return ((static_cast<int>(numToRound) / multiple) * multiple);
}

void COverlayLoader::setOverlaySettings(OverlaySettings overlaySettings)
{ 
    settings = overlaySettings; 

    // Hard-code for testing
    settings.offset = -125;
    settings.xAxis = 2;
    settings.yAxis = 0;
    settings.zAxis = 1;
    settings.scale = 4;
    settings.chunkSize = 25;
}

auto COverlayLoader::determineChunks(cv::Rect rect, int zIndex) const -> OverlayChunkIDs
{
    OverlayChunkIDs res;
    
    if (settings.path.size() == 0) {
        return {};
    }
 
    auto xIndexStart = std::max(100, roundDownToNearestMultiple((rect.x - 100) / settings.scale, settings.chunkSize) - settings.offset);
    xIndexStart -= settings.chunkSize; // due to the fact that file 000100 contains from -100 to 100, 000125 contains from 0 to 200, 000150 from 100 to 300
    auto yIndexStart = std::max(100, roundDownToNearestMultiple((rect.y - 100) / settings.scale, settings.chunkSize) - settings.offset);
    yIndexStart -= settings.chunkSize;

    auto zIndexEnd = std::max(100, roundDownToNearestMultiple((zIndex - 100) / settings.scale, settings.chunkSize) - settings.offset);
    auto zIndexStart = zIndexEnd - settings.chunkSize;

    auto xIndexEnd = roundDownToNearestMultiple((rect.br().x - 100) / settings.scale, settings.chunkSize) - settings.offset;
    auto yIndexEnd = roundDownToNearestMultiple((rect.br().y - 100) / settings.scale, settings.chunkSize) - settings.offset;

    OverlayChunkID id;
    for (auto z = zIndexStart; z <= zIndexEnd; z += settings.chunkSize) {
        for (auto x = xIndexStart; x <= xIndexEnd; x += settings.chunkSize) {
            for (auto y = yIndexStart; y <= yIndexEnd; y += settings.chunkSize) {
                id[settings.xAxis] = x;
                id[settings.yAxis] = y;
                id[settings.zAxis] = z;

                res.push_back(id);
            }
        }
    }

    return res;
}

auto COverlayLoader::determineNotLoadedFiles(OverlayChunkIDs chunks) const -> OverlayChunkFiles
{
    QString folder;
    OverlayChunkFiles fileList;
    QDir overlayMainFolder(QString::fromStdString(settings.path));
    auto absPath = overlayMainFolder.absolutePath();

    for (auto chunk : chunks) {
        if (chunkData.find(chunk) == chunkData.end()) {
            // TODO:Check if the settings logic for axis really works here
            folder = QStringLiteral("%1")
                         .arg(chunk[settings.yAxis], 6, 10, QLatin1Char('0'))
                         .append("_" + QStringLiteral("%1").arg(chunk[settings.zAxis], 6, 10, QLatin1Char('0')))
                         .append("_" + QStringLiteral("%1").arg(chunk[settings.xAxis], 6, 10, QLatin1Char('0')));

            QDir overlayFolder(absPath + QDir::separator() + folder);
            QStringList files = overlayFolder.entryList({"*.ply", "*.obj"}, QDir::NoDotAndDotDot | QDir::Files);

            for (auto file : files) {
                file = overlayFolder.path() + QDir::separator() + file;
                fileList[chunk].push_back(file.toStdString());
            }
        }
    }

    return fileList;
}

void COverlayLoader::loadOverlayData(OverlayChunkFiles chunksToLoad)
{
    if (chunksToLoad.size() == 0 || settings.path.size() == 0) {
        return;
    }

    // Required since during an OFS run multiple threads might try to start loading data
    std::lock_guard<std::shared_mutex> lock(loadMutex);

    vc::ITKMesh::Pointer mesh;
    itk::Point<double, 3> point;
    threadData.clear();

    // Convert to flat work list for threads
    std::vector<OverlayChunkID> chunks;
    std::vector<std::string> fileNames;
    for (auto chunk : chunksToLoad) {
        for (auto file : chunk.second) {
            chunks.push_back(chunk.first);
            fileNames.push_back(file);
        }
    }

    int numThreads = static_cast<int>(std::thread::hardware_concurrency());
    int jobSize = 5;
    for (int f = 0; f < fileNames.size(); f += numThreads * jobSize) {
        std::vector<std::thread> threads;

        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([=]() {
                for (int j = 0; j < jobSize && (f + i * jobSize + j) < fileNames.size(); ++j) {
                    loadSingleOverlayFile(fileNames.at(f + i * jobSize + j), chunks.at(f + i * jobSize + j), i);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }
    }

    for (auto threadDataSet : threadData) {
        if (threadDataSet.second.size() > 0) {
            mergeThreadData(threadDataSet.second);
        }
    }
}

static bool ends_with(std::string_view str, std::string_view suffix)
{
    return str.size() >= suffix.size() && str.compare(str.size()-suffix.size(), suffix.size(), suffix) == 0;
}

void COverlayLoader::loadSingleOverlayFile(const std::string& file, OverlayChunkID chunkID, int threadNum) const
{
    vc::ITKMesh::Pointer mesh;
    itk::Point<double, 3> point;

    //std::cout << file.toStdString() << std::endl;
    if (ends_with(file, ".ply")) {
        volcart::io::PLYReader reader(fs::path({file}));
        reader.read();
        mesh = reader.getMesh();
    } else if (ends_with(file, ".obj")) {
        volcart::io::OBJReader reader;
        reader.setPath(file);
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

        if (point[settings.xAxis] >= 0 && point[settings.yAxis] >= 0 && point[settings.zAxis] >= 0) {
            // Just a type conversion for the point, so do not use the axis settings here
            threadData[threadNum][chunkID].push_back({point[0], point[1], point[2]});
            //threadSliceData[threadNum][chunkID][point[settings.zAxis]].push_back({point[settings.xAxis], point[settings.yAxis]});
        }
    }
}

void COverlayLoader::mergeThreadData(OverlayChunkData threadData) const
{
    std::lock_guard<std::shared_mutex> lock(mergeMutex);

    for (auto it = threadData.begin(); it != threadData.end(); ++it) {
        std::pair<OverlayChunkData::iterator, bool> ins = chunkData.insert(*it);
        if (!ins.second) {  
            // Map key already existed, so we have to merge the slice data
            OverlayData* vec1 = &(it->second);
            OverlayData* vec2 = &(ins.first->second);
            vec2->insert(vec2->end(), vec1->begin(), vec1->end());
        }
    }

    // for (auto& data : chunkData) {
    //     std::sort(
    //         data.second.begin(), data.second.end(),
    //         [](const auto& a, const auto& b) { 
    //             return (a[0] < b[0]) || (a[0] == b[0] && a[1] < b[1]) || (a[0] == b[0] && a[1] == b[1] && a[2] < b[2]);
    //         }
    //     );
    //     data.second.erase(
    //         std::unique(data.second.begin(), data.second.end()),
    //         data.second.end());
    // }

    // for (auto it = threadSliceData.begin(); it != threadSliceData.end(); ++it) {
    //     for (auto jt = it->second.begin(); jt != it->second.end(); ++jt) {
           
    //         std::pair<OverlayChunkSliceData::iterator, bool> ins = chunkSliceData.insert(*jt);
    //         if (!ins.second) { 
    //             // Map key already existed, so we have to merge the slice data
    //             for (auto data : jt->second) {
    //                 chunkSliceData[jt->first][data.first].insert(chunkSliceData[jt->first][data.first].end(), data.second.begin(), data.second.end());
    //             }

    //             // OverlaySliceData* vec1 = &(jt->second);
    //             // OverlaySliceData* vec2 = &(ins.first->second);
    //             // vec2->insert(vec2->end(), vec1->begin(), vec1->end());
    //         }
    //     }
    // }
}

// auto COverlayLoader::getOverlayData(cv::Rect rect) const -> OverlayChunkDataRef
// {
//     OverlayChunkDataRef res;
//     auto chunks = determineChunks(rect);

//     for (auto chunk : chunks) {
//         res[chunk] = &chunkData[chunk];
//     }

//     return res;
// }

auto COverlayLoader::getOverlayData(cv::Rect rect, int zIndex) -> OverlaySliceData
{
    OverlaySliceData res;
    auto chunks = determineChunks(rect, zIndex);
    loadOverlayData(determineNotLoadedFiles(chunks));

    // for (auto chunk : chunks) {
    //     if (chunkSliceData[chunk].find(zIndex) != chunkSliceData[chunk].end()) {
    //         for (auto point : chunkSliceData[chunk][zIndex]) {
    //             res.push_back({point[0], point[1]});
    //         }
    //     }
    // }

    for (auto chunk : chunks) {
        for (auto point : chunkData[chunk]) {
            if (point[settings.zAxis] == zIndex) {
                res.push_back({point[settings.xAxis], point[settings.yAxis]});
            }
        }
    }

    // There are quite some point duplicates across chunks, so we remove them
    // now for faster rendering and processing.
    std::sort(res.begin(), res.end(), [](const auto& a, const auto& b) {
        return (a[0] < b[0]) || (a[0] == b[0] && a[1] < b[1]);
    });
    res.erase(std::unique(res.begin(), res.end()), res.end());

    return res;
}