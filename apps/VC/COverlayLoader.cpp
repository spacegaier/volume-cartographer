// COverlayLoader.cpp
// Philip Allgaier 2024 May
#include "COverlayLoader.hpp"
#include "CVolumeViewer.hpp"

#include "CVolumeViewer.hpp"
#include "vc/core/io/OBJReader.hpp"
#include "vc/core/io/PLYReader.hpp"
#include "vc/core/types/ITKMesh.hpp"

#include "ui_VCOverlayImportDlg.h"

using namespace ChaoVis;
namespace vc = volcart;
namespace fs = std::filesystem;

COverlayLoader::COverlayLoader()
{}

auto roundDownToNearestMultiple(float numToRound, int multiple)  ->int
{
    return ((static_cast<int>(numToRound) / multiple) * multiple);
}

void COverlayLoader::resetData()
{
    settings = OverlaySettings();
    chunkSliceData.clear();
}

void COverlayLoader::showOverlayImportDlg(const std::string& path, CVolumeViewer* viewer)
{
    if (path.size() == 0) {
        return;
    }

    QString qtPath = QString::fromStdString(path);

    auto dlg = new QDialog();
    auto overlayImportDlg = new Ui::VCOverlayImportDlg();
    overlayImportDlg->setupUi(dlg);
    QObject::connect(overlayImportDlg->comboBoxNamePattern, &QComboBox::currentIndexChanged, dlg, [this, qtPath, dlg, overlayImportDlg, viewer]() {
        // Set defaults for patterns        
        if (overlayImportDlg->comboBoxNamePattern->currentIndex() == 0) {
            overlayImportDlg->spinBoxOffset->setValue(-125);
            overlayImportDlg->comboBox1stAxis->setCurrentIndex(1);
            overlayImportDlg->comboBox2ndAxis->setCurrentIndex(2);
            overlayImportDlg->comboBox3rdAxis->setCurrentIndex(0);
            overlayImportDlg->doubleSpinBoxScalingFactor->setValue(4);
            overlayImportDlg->spinBoxChunkSize->setValue(25);

        } else if (overlayImportDlg->comboBoxNamePattern->currentIndex() == 1) {
            overlayImportDlg->spinBoxOffset->setValue(-500);
            overlayImportDlg->comboBox1stAxis->setCurrentIndex(1);
            overlayImportDlg->comboBox2ndAxis->setCurrentIndex(2);
            overlayImportDlg->comboBox3rdAxis->setCurrentIndex(0);
            overlayImportDlg->doubleSpinBoxScalingFactor->setValue(1);
            overlayImportDlg->spinBoxChunkSize->setValue(200);
        }
    });
    // Enforce change so that matching values are set by callback
    overlayImportDlg->comboBoxNamePattern->setCurrentIndex(-1);
    overlayImportDlg->comboBoxNamePattern->setCurrentIndex(0);

    QObject::connect(overlayImportDlg->buttonBox, &QDialogButtonBox::accepted, dlg, [this, qtPath, dlg, overlayImportDlg, viewer]() {
        OverlaySettings overlaySettings;
        overlaySettings.singleFile = qtPath.endsWith(".ply") || qtPath.endsWith(".obj");
        overlaySettings.path = qtPath.toStdString();
        overlaySettings.namePattern = overlayImportDlg->comboBoxNamePattern->currentIndex();
        overlaySettings.xAxis = overlayImportDlg->comboBox1stAxis->currentText() == "X" ? 0 : overlayImportDlg->comboBox2ndAxis->currentText() == "X" ? 1 : 2;
        overlaySettings.yAxis = overlayImportDlg->comboBox1stAxis->currentText() == "Y" ? 0 : overlayImportDlg->comboBox2ndAxis->currentText() == "Y" ? 1 : 2;
        overlaySettings.zAxis = overlayImportDlg->comboBox1stAxis->currentText() == "Z" ? 0 : overlayImportDlg->comboBox2ndAxis->currentText() == "Z" ? 1 : 2;
        overlaySettings.offset = overlayImportDlg->spinBoxOffset->value();
        overlaySettings.scale = overlayImportDlg->doubleSpinBoxScalingFactor->value();
        overlaySettings.chunkSize = overlayImportDlg->spinBoxChunkSize->value();

        setOverlaySettings(overlaySettings);
        viewer->ScheduleOverlayUpdate();
        dlg->close();
    });
    QObject::connect(overlayImportDlg->buttonBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    dlg->show();
}

void COverlayLoader::setOverlaySettings(OverlaySettings overlaySettings)
{ 
    settings = overlaySettings;
}

auto COverlayLoader::determineChunks(cv::Rect rect, int zIndex) const -> OverlayChunkIDs
{
    OverlayChunkIDs res;
    
    if (settings.path.size() == 0) {
        return {};
    }

    int xIndexStart, yIndexStart, zIndexStart;
    int xIndexEnd, yIndexEnd, zIndexEnd;
    int step;
 
    if (settings.namePattern == 0) {
        xIndexStart = std::max(100, roundDownToNearestMultiple((rect.x - 100) / settings.scale, settings.chunkSize) - settings.offset);
        xIndexStart -= settings.chunkSize; // due to the fact that file 000100 contains from -100 to 100, 000125 contains from 0 to 200, 000150 from 100 to 300
        yIndexStart = std::max(100, roundDownToNearestMultiple((rect.y - 100) / settings.scale, settings.chunkSize) - settings.offset);
        yIndexStart -= settings.chunkSize;

        zIndexEnd = std::max(100, roundDownToNearestMultiple((zIndex - 100) / settings.scale, settings.chunkSize) - settings.offset);
        zIndexStart = zIndexEnd - settings.chunkSize;

        xIndexEnd = roundDownToNearestMultiple((rect.br().x - 100) / settings.scale, settings.chunkSize) - settings.offset;
        yIndexEnd = roundDownToNearestMultiple((rect.br().y - 100) / settings.scale, settings.chunkSize) - settings.offset;

        step = settings.chunkSize;

    } else if (settings.namePattern == 1) {
        // The cells number the chunks sequentially, opposed to the pattern above, where the chunk names are the coordinates
        xIndexStart = std::max(1, (rect.x - settings.offset) / settings.chunkSize);
        yIndexStart = std::max(1, (rect.y - settings.offset) / settings.chunkSize);
        
        zIndexStart = std::max(1, (zIndex - settings.offset) / settings.chunkSize);
        zIndexEnd = zIndexStart;

        xIndexEnd = (rect.br().x - settings.offset) / settings.chunkSize;
        yIndexEnd = (rect.br().y - settings.offset) / settings.chunkSize;      

        step = 1;  
    }

    OverlayChunkID id;
    for (auto z = zIndexStart; z <= zIndexEnd; z += step) {
        for (auto x = xIndexStart; x <= xIndexEnd; x += step) {
            for (auto y = yIndexStart; y <= yIndexEnd; y += step) {
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
    QString folder, fileName;
    OverlayChunkFiles fileList;
    QDir overlayMainFolder(QString::fromStdString(settings.path));
    auto absPath = overlayMainFolder.absolutePath();

    for (auto chunk : chunks) {
        if (chunkSliceData.find(chunk) == chunkSliceData.end()) {
            if (settings.namePattern == 0) {
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
            } else if (settings.namePattern == 1) {
                fileName = QStringLiteral("cell_yxz_%1")
                            .arg(chunk[settings.yAxis], 3, 10, QLatin1Char('0'))
                            .append("_" + QStringLiteral("%1").arg(chunk[settings.xAxis], 3, 10, QLatin1Char('0')))
                            .append("_" + QStringLiteral("%1.ply").arg(chunk[settings.zAxis], 3, 10, QLatin1Char('0')));

                QDir overlayFile(absPath + QDir::separator() + fileName);
                if (QFileInfo::exists(overlayFile.path())) {
                    std::cout << "File: " << overlayFile.path().toStdString() << std::endl;
                    fileList[chunk].push_back(overlayFile.path().toStdString());
                }
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
    threadSliceData.clear();

    vc::ITKMesh::Pointer mesh;
    itk::Point<double, 3> point;

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

    // Ensures that for each thread we have a map entry existing
    for (int i = 0; i < numThreads; ++i) {
        threadSliceData[i].clear();
    }

    // TODO: Test different job sizes
    // Set a minimum to prevent too many way too small threads and a maximum to
    // have the option later to interrupt the process at the join().
    int jobSize = std::clamp(static_cast<int>(chunksToLoad.size() / numThreads), 5, 20);
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

    mergeThreadData();
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
    auto it = threadSliceData.find(threadNum)->second.find(chunkID);
    if (it == threadSliceData.find(threadNum)->second.end()) {
        threadSliceData[threadNum][chunkID].clear();
        it = threadSliceData.find(threadNum)->second.find(chunkID);
    }
    auto estimatedPointsPerDim = numPoints / 3;

    for (std::uint64_t pnt_id = 0; pnt_id < numPoints; pnt_id++) {
        point = mesh->GetPoint(pnt_id);
        point[0] += settings.offset;
        point[1] += settings.offset;
        point[2] += settings.offset;
        point[0] *= settings.scale;
        point[1] *= settings.scale;
        point[2] *= settings.scale;

        if (point[0] >= 0 && point[1] >= 0 && point[2] >= 0) {
            // it->second.push_back({point[settings.xAxis], point[settings.yAxis], point[settings.zAxis]});
            auto jt = it->second.find(point[settings.zAxis]);
            if (jt == it->second.end()) {
                it->second[point[settings.zAxis]].reserve(estimatedPointsPerDim);
                // Now it has to exist
                jt = it->second.find(point[settings.zAxis]);
            }
            jt->second.push_back({(float)point[settings.xAxis], (float)point[settings.yAxis]});
        }
    }
}

void COverlayLoader::mergeThreadData() const
{
    std::lock_guard<std::shared_mutex> lock(mergeMutex);

    for (auto it = threadSliceData.begin(); it != threadSliceData.end(); ++it) {
        for (auto jt = it->second.begin(); jt != it->second.end(); ++jt) {
           
            std::pair<OverlayChunkSliceData::iterator, bool> ins = chunkSliceData.insert(*jt);
            if (!ins.second) { 
                // Map key already existed, so we have to merge the slice data
                for (auto& data : jt->second) {
                    chunkSliceData[jt->first][data.first].insert(chunkSliceData[jt->first][data.first].end(), data.second.begin(), data.second.end());
                }
            }
        }
    }
}

auto COverlayLoader::getOverlayData(cv::Rect2d rect, int zIndex) -> OverlaySliceData
{
    OverlaySliceData res;
    if (settings.path.size() == 0) {
        return res;
    }

    auto chunks = determineChunks(rect, zIndex);
    loadOverlayData(determineNotLoadedFiles(chunks));

    cv::Point2f point2f;
    for (auto& chunk : chunks) {
        auto it = chunkSliceData.find(chunk);
        if (it != chunkSliceData.end()) {
            auto zt = it->second.find(zIndex);
            if (zt != it->second.end()) {
                for (auto& point : zt->second) {
                    point2f = cv::Point2f(point.x, point.y);
                    if (rect.contains(point2f))
                        res.push_back(point2f);
                }
            }
        }
    }

    // There are quite some point duplicates across chunks, so we remove them
    // now for faster rendering and processing.
    std::sort(res.begin(), res.end(), [](const auto& a, const auto& b) {
        return (a.x < b.x) || (a.x == b.x && a.y < b.y);
    });
    res.erase(std::unique(res.begin(), res.end()), res.end());

    return res;
}