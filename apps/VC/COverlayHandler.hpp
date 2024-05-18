// COverlayHandler.hpp
// Philip Allgaier 2024 May
#pragma once

#include <map>
#include <shared_mutex>

#include <QString>
#include <opencv2/imgproc.hpp>

namespace ChaoVis
{
class CVolumeViewer;
class COverlayGraphicsItem;

typedef std::vector<cv::Vec2d> OverlaySliceData;
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

    void loadSingleOverlayFile(QString path) const;
    void mergeThreadData(OverlayData threadData) const;

protected:
    CVolumeViewer* viewer;
    OverlaySettings settings;

    mutable OverlayData data;
    mutable std::shared_mutex dataMutex;
};

}