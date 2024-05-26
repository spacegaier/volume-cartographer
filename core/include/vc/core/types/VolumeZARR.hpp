#pragma once

/** @file */

#include "vc/core/types/Volume.hpp"

#include "z5/dataset.hxx"
#include "z5/filesystem/handle.hxx"
#include "xtensor/xtensor.hpp"

namespace volcart
{

typedef cv::Vec<unsigned long, 3> ChunkID;
typedef std::vector<cv::Vec2d> ChunkSlice;
typedef xt::xtensor<uint16_t, 3> Chunck;

struct cmpChunkID {
    bool operator()(const ChunkID& a, const ChunkID& b) const
    {
        return a[0] < b[0] || (a[0] == b[0] && a[1] < b[1]) || (a[0] == b[0] && a[1] == b[1] && a[2] < b[2]);
    }
};

typedef std::map<ChunkID, Chunck*, cmpChunkID> ChunkData;
typedef std::map<ChunkID, std::vector<std::string>, cmpChunkID> ChunkFiles;

typedef z5::types::ShapeType ChunkRequest;
typedef std::vector<ChunkRequest> ChunkRequests;

struct ChunkDataSettings {
    std::string path;
    int xAxis;
    int yAxis;
    int zAxis;
    int offset;
    float scale;
    int chunkSize; // logical chunk size on disk before applying "scale"
};

/**
 * @class VolumeZARR
 * @author Philip Allgaier
 *
 * @brief Volumetric ZARR image data
 *
 * @ingroup Types
 */
class VolumeZARR : public Volume
{
public:
    /** Shared pointer type */
    using Pointer = std::shared_ptr<VolumeZARR>;

    /** @brief Load the Volume from a directory path */
    explicit VolumeZARR(volcart::filesystem::path path);

    /** @brief Make a new Volume at the specified path */
    VolumeZARR(volcart::filesystem::path path, Identifier uuid, std::string name);

    /**@{*/
    /** @brief Get the file path of a slice by index */
    volcart::filesystem::path getSlicePath(int index) const override;
    /**@}*/

    cv::Mat getSliceData(int index, VolumeAxis axis = Z) const override;

    cv::Mat getSliceDataRect(int index, cv::Rect rect, VolumeAxis axis = Z) const override;

    void setSliceData(int index, const cv::Mat& slice, bool compress = true) override;

    /**@{*/
    /** @brief Get ZARR file levels*/
    std::vector<std::string> zarrLevels() const;
    /**@}*/

    /**@{*/
    /** @brief Set desired ZARR level */
    void setZarrLevel(int level);
    /**@}*/

    void openZarr();

    void setChunkSettings(ChunkDataSettings chunkSettings);
    auto determineChunksForRect(int index, cv::Rect rect) const -> ChunkData;
    auto determineNotLoadedChunks(int index, cv::Rect rect) const -> ChunkRequests;
    void loadChunkFiles(ChunkRequests requests, VolumeAxis axis) const;
    auto getChunkData(int index, cv::Rect rect, VolumeAxis axis) -> ChunkData;
    auto getChunkSliceData(int index, cv::Rect rect) const -> ChunkSlice;

    void loadSingleChunkFile(std::string file, ChunkID chunkID, int threadNum) const;
    void mergeThreadData(ChunkData threadData) const;

protected:
    ChunkDataSettings settings;

    /** ZARR file*/
    z5::filesystem::handle::File zarrFile_;
    /** ZARR data set*/
    std::unique_ptr<z5::Dataset> zarrDs_{nullptr};
    /** Loaded chunks */
    mutable std::map<VolumeAxis, std::map<unsigned int, xt::xtensor<uint16_t, 3>*>> loadedChunks_;
    /** Active ZARR level */
    int zarrLevel_{-1};

    nlohmann::json groupAttr_;

    mutable ChunkData chunkData;
    mutable std::shared_mutex dataMutex;
    // Each numbered thread fills its own overlay data and at the end
    // of the data loading they will get merged together
    mutable std::map<int, ChunkData> threadData;

    /** Load slice from disk */
    cv::Mat load_slice_(int index, VolumeAxis axis = Z) const;
    cv::Mat load_slice_(int index, cv::Rect rect = cv::Rect(), VolumeAxis axis = Z) const;
};
}  // namespace volcart
