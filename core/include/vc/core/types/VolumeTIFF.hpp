#pragma once

/** @file */

#include "vc/core/types/Volume.hpp"

namespace volcart
{

/**
 * @class VolumeTIFF
 * @author Philip Allgaier
 *
 * @brief Volumetric TIFF image data
 *
 * @ingroup Types
 */
class VolumeTIFF : public Volume
{
public:
    /** Shared pointer type */
    using Pointer = std::shared_ptr<VolumeTIFF>;

    /** @brief Load the Volume from a directory path */
    explicit VolumeTIFF(volcart::filesystem::path path);

    /** @brief Make a new Volume at the specified path */
    VolumeTIFF(volcart::filesystem::path path, Identifier uuid, std::string name);

    /**@{*/
    /** @brief Get the file path of a slice by index */
    volcart::filesystem::path getSlicePath(int index) const override;
    /**@}*/

    cv::Mat getSliceData(int index, VolumeAxis axis = Z) const override;

    cv::Mat getSliceDataRect(int index, cv::Rect rect, VolumeAxis axis = Z) const override; 

    void setSliceData(int index, const cv::Mat& slice, bool compress = true) override;

protected:
    /** Load slice from disk */
    cv::Mat load_slice_(int index, VolumeAxis axis = Z) const;
    /** Load slice from cache */
    cv::Mat cache_slice_(int index) const;
};
}  // namespace volcart
