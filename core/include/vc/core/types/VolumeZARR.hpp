#pragma once

/** @file */

#include "vc/core/types/Volume.hpp"

#include "z5/dataset.hxx"
#include "z5/filesystem/handle.hxx"
#include "xtensor/xtensor.hpp"

namespace volcart
{

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

protected:
    /** ZARR file*/
    z5::filesystem::handle::File zarrFile_;
    /** ZARR data set*/
    std::unique_ptr<z5::Dataset> zarrDs_{nullptr};
    /** Loaded chunks */
    mutable std::map<VolumeAxis, std::map<unsigned int, xt::xtensor<uint16_t, 3>*>> loadedChunks_;
    /** Active ZARR level */
    int zarrLevel_{-1};

    /** Load slice from disk */
    cv::Mat load_slice_(int index, VolumeAxis axis = Z) const override;
    /** Load slice from cache */
    cv::Mat cache_slice_(int index) const override;
};
}  // namespace volcart
