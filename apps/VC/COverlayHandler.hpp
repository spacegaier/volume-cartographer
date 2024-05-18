// COverlayHandler.hpp
// Philip Allgaier 2024 May
#pragma once

#include <map>
#include <QString>
#include <opencv2/imgproc.hpp>

namespace ChaoVis
{
class CVolumeViewer;

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
    auto getOverlayData() const -> std::map<int, std::vector<cv::Vec2d>> { return data; }
    void updateOverlayData();

protected:
    CVolumeViewer* viewer;
    OverlaySettings settings;

    std::map<int, std::vector<cv::Vec2d>> data;
};

}