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

typedef cv::Vec3i OverlayChunkID;
typedef std::vector<OverlayChunkID> OverlayChunkIDs;
typedef std::vector<cv::Vec2d> OverlaySliceData;
typedef std::vector<cv::Vec3d> OverlayData;

struct cmpOverlayChunkID {
    bool operator()(const OverlayChunkID& a, const OverlayChunkID& b) const
    {
        return a[0] < b[0] || (a[0] == b[0] && a[1] < b[1]) || (a[0] == b[0] && a[1] == b[1] && a[2] < b[2]);
    }
};

typedef std::map<OverlayChunkID, OverlayData, cmpOverlayChunkID> OverlayChunkData;
typedef std::map<OverlayChunkID, OverlayData*, cmpOverlayChunkID> OverlayChunkDataRef;
typedef std::map<OverlayChunkID, std::vector<QString>, cmpOverlayChunkID> OverlayChunkFiles;

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
    auto determineChunksForView() const -> OverlayChunkIDs;
    auto determineNotLoadedOverlayFiles() const -> OverlayChunkFiles;
    void loadOverlayData(OverlayChunkFiles files);
    auto getAllOverlayData() const -> OverlayChunkData { return chunkData; }
    auto getOverlayDataForView() const -> OverlayChunkDataRef;
    auto getOverlayDataForView(int zIndex = -1) const -> OverlaySliceData;
    void updateOverlayData();

    void loadSingleOverlayFile(const QString& file, OverlayChunkID chunkID, int threadNum) const;
    void mergeThreadData(OverlayChunkData threadData) const;

protected:
    CVolumeViewer* viewer;
    OverlaySettings settings;

    mutable OverlayChunkData chunkData;
    mutable std::shared_mutex dataMutex;
    // Each numbered thread fills its own overlay data and at the end
    // of the data loading they will get merged together
    mutable std::map<int, OverlayChunkData> threadData;   
};

}