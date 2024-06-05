#pragma once

/** @file */

#include "vc/core/types/Volume.hpp"

#include "z5/dataset.hxx"
#include "z5/filesystem/handle.hxx"
#include "xtensor/xtensor.hpp"

namespace volcart
{

struct KeyHasher {
    // Taken from https://stackoverflow.com/a/72073933
    std::size_t operator()(z5::types::ShapeType const& vec) const
    {
        std::size_t seed = vec.size();
        for (auto x : vec) {
            x = ((x >> 16) ^ x) * 0x45d9f3b;
            x = ((x >> 16) ^ x) * 0x45d9f3b;
            x = (x >> 16) ^ x;
            seed ^= x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
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

    using DefaultCache = LRUCache<z5::types::ShapeType, std::vector<std::uint16_t>, KeyHasher>;

    /** @brief Load the Volume from a directory path */
    explicit VolumeZARR(volcart::filesystem::path path);

    virtual bool isChunked() const { return true; };

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
    std::vector<int> getZarrLevels() const;
    /**@}*/

    /**@{*/
    /** @brief Set desired ZARR level */
    void setZarrLevel(int level) { zarrLevel_ = level; }
    /** @brief Get the currently used ZARR level */
    auto getZarrLevel() const -> int { return zarrLevel_; }
    /** @brief Get scale factor for ZARR level*/
    auto getScaleForLevel(int level) const -> float;
    /**@}*/

    void openZarr();

        /** @brief Purge the slice cache */
    void cachePurge() const override;

    void putCacheChunk(z5::types::ShapeType chunkId, void* chunk) const;
    void* getCacheChunk(z5::types::ShapeType chunkId) const;

    /** Format: X, Y, Z */
    auto getSize(int level) -> cv::Vec3i { return cv::Vec3i(
        zarrDs_[level]->shape()[1],
        zarrDs_[level]->shape()[2],
        zarrDs_[level]->shape()[0]); }

    auto getCurrentSize() -> cv::Vec3i { return getSize(zarrLevel_); }


protected:
    /** ZARR file*/
    z5::filesystem::handle::File zarrFile_;
    /** ZARR data set*/
    std::map<int, std::unique_ptr<z5::Dataset>> zarrDs_;
    /** Active ZARR level */
    int zarrLevel_{-1};

    nlohmann::json groupAttr_;

    /** Chunk cache */
    // mutable DefaultCache::Pointer cache_{DefaultCache::New(1000L)};
    mutable std::map<int, DefaultCache::Pointer> caches_;

    /** Load slice from disk */
    cv::Mat load_slice_(int index, cv::Rect2i rect = cv::Rect2i(), VolumeAxis axis = Z) const;
};
}  // namespace volcart
