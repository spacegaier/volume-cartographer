// COverlayLoader.hpp
// Philip Allgaier 2024 May
#pragma once

#include <map>
#include <set>
#include <shared_mutex>

#include <opencv2/imgproc.hpp>

namespace ChaoVis
{
// struct cmpVec2d {
//     bool operator()(const cv::Vec2d& a, const cv::Vec2d& b) const
//     {
//         return (a[0] < b[0]) || (a[0] == b[0] && a[1] < b[1]);
//     }
// };

// struct cmpVec3d {
//     bool operator()(const cv::Vec3d& a, const cv::Vec3d& b) const
//     {
//         return (a[0] < b[0]) || (a[0] == b[0] && a[1] < b[1]) || (a[0] == b[0] && a[1] == b[1] && a[2] < b[2]);
//     }
// };

typedef cv::Vec3i OverlayChunkID;
typedef std::vector<OverlayChunkID> OverlayChunkIDs;
typedef std::vector<cv::Vec2d> OverlaySliceData;
typedef std::vector<cv::Vec3d> OverlayData;
// typedef std::set<cv::Vec2d, cmpVec2d> OverlaySliceData;
// typedef std::set<cv::Vec3d, cmpVec3d> OverlayData;

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
        std::string path;
        int xAxis;
        int yAxis;
        int zAxis;
        int offset;
        float scale;
        int chunkSize; // logical chunk size on disk before applying "scale"
    };

    void setOverlaySettings(OverlaySettings overlaySettings);
    auto determineChunks(cv::Rect rect, int zIndex) const -> OverlayChunkIDs;
    auto determineNotLoadedFiles(OverlayChunkIDs chunks) const -> OverlayChunkFiles;
    void loadOverlayData(OverlayChunkFiles files);
    // auto getOverlayData(cv::Rect rect) const -> OverlayChunkDataRef;
    auto getOverlayData(cv::Rect rect, int zIndex = -1) -> OverlaySliceData;

    void loadSingleOverlayFile(const std::string& file, OverlayChunkID chunkID, int threadNum) const;
    void mergeThreadData(OverlayChunkData threadData) const;

protected:
    OverlaySettings settings;

    mutable OverlayChunkData chunkData;
    mutable OverlayChunkSliceData chunkSliceData;
    mutable std::shared_mutex loadMutex;
    mutable std::shared_mutex mergeMutex;
    // Each numbered thread fills its own overlay data structure in the map
    // and at the end of the data loading they will get merged together
    mutable std::map<int, OverlayChunkData> threadData;   
    mutable std::map<int, OverlayChunkSliceData> threadSliceData;   
};

}