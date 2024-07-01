// COverlayLoader.hpp
// Philip Allgaier 2024 May
#pragma once

#include <map>
#include <set>
#include <shared_mutex>

#include <opencv2/imgproc.hpp>

namespace ChaoVis
{
    class CVolumeViewer;

struct cmpPoint2f {
    bool operator()(const cv::Point2f& a, const cv::Point2f& b) const
    {
        return (a.x < b.x) || (a.x == b.x && a.y < b.y);
    }
};

typedef cv::Vec3i OverlayChunkID;
typedef std::vector<OverlayChunkID> OverlayChunkIDs;
typedef std::vector<cv::Point2f> OverlaySliceData;
typedef std::vector<cv::Point3f> OverlayData;

struct cmpOverlayChunkID {
    bool operator()(const OverlayChunkID& a, const OverlayChunkID& b) const
    {
        return a[0] < b[0] || (a[0] == b[0] && a[1] < b[1]) || (a[0] == b[0] && a[1] == b[1] && a[2] < b[2]);
    }
};

typedef std::map<OverlayChunkID, OverlayData, cmpOverlayChunkID> OverlayChunkData;
typedef std::map<OverlayChunkID, std::map<int, OverlaySliceData>, cmpOverlayChunkID> OverlayChunkSliceData;
typedef std::map<OverlayChunkID, OverlayData*, cmpOverlayChunkID> OverlayChunkDataRef;
typedef std::map<OverlayChunkID, std::vector<std::string>, cmpOverlayChunkID> OverlayChunkFiles;

class COverlayLoader
{
public:
    COverlayLoader();

    struct OverlaySettings {
        bool singleFile;
        std::string path;
        int namePattern; // 0 = Thaumato Layers, 1 = Thaumato Cells
        int xAxis;
        int yAxis;
        int zAxis;
        int offset;
        float scale;
        int chunkSize; // logical chunk size on disk before applying "scale"
    };

    void showOverlayImportDlg(const std::string& path, CVolumeViewer* viewer);
    void setOverlaySettings(OverlaySettings overlaySettings);
    auto determineChunks(cv::Rect rect, int zIndex) const -> OverlayChunkIDs;
    auto determineNotLoadedFiles(OverlayChunkIDs chunks) const -> OverlayChunkFiles;
    void loadOverlayData(OverlayChunkFiles files);
    auto getOverlayData(cv::Rect2d rect, int zIndex = -1) -> OverlaySliceData;

    void loadSingleOverlayFile(const std::string& file, OverlayChunkID chunkID, int threadNum) const;
    void mergeThreadData() const;

    void resetData();

protected:
    OverlaySettings settings;

    mutable OverlayChunkSliceData chunkSliceData;
    mutable std::shared_mutex loadMutex;
    mutable std::shared_mutex mergeMutex;
    // Each numbered thread fills its own overlay data structure in the map
    // and at the end of the data loading they will get merged together
    mutable std::map<int, OverlayChunkSliceData> threadSliceData;   
};

}