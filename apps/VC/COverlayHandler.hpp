// COverlayHandler.hpp
// Philip Allgaier 2024 May
#pragma once

#include <map>
#include <set>
#include <shared_mutex>

#include <QString>
#include <opencv2/imgproc.hpp>

namespace ChaoVis
{
class CVolumeViewer;
class COverlayGraphicsItem;

typedef std::vector<cv::Vec2d> OverlaySliceData;
// Map of Z index and overlay point data
typedef std::map<int, OverlaySliceData> OverlayData;

class COverlayHandler
{
public:
    COverlayHandler(CVolumeViewer* viewer);

    struct OverlaySettings {
        QString path;
        int xAxis;
        int yAxis;
        int zAxis;
        int offset;
        float scale;
        int chunkSize; // logical chunk size on disk before applying "scale"
    };

    void setOverlaySettings(OverlaySettings overlaySettings);
    auto determineOverlayFiles() -> QStringList;
    void loadOverlayData(QStringList files);
    auto getOverlayData() const -> OverlayData { return data; }
    void updateOverlayData();

    void loadSingleOverlayFile(QString file, int threadNum) const;
    void mergeThreadData(OverlayData threadData) const;

protected:
    CVolumeViewer* viewer;
    OverlaySettings settings;

    mutable OverlayData data;
    mutable std::shared_mutex dataMutex;
    // Each numbered thread fills its own overlay data and at the end
    // of the data loading they will get merged together
    mutable std::map<int, OverlayData> threadData;

    std::set<QString> loadedFiles;
};

}