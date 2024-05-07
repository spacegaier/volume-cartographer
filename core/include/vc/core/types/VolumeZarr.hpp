#pragma once

/** @file */

#include <mutex>

#include "vc/core/types/Volume.hpp"

namespace volcart
{
/**
 * @class VolumeZarr
 * @author Philip Allgaier
 *
 * @brief Volumetric ZARR
 *
 * @ingroup Types
 */
class VolumeZarr : public Volume
{
public:
    /** @brief Load the Volume from a directory path */
    explicit VolumeZarr(volcart::filesystem::path path);

    /** @brief Make a new Volume at the specified path */
    VolumeZarr(volcart::filesystem::path path, Identifier uuid, std::string name);

    /** @overload Volume(volcart::filesystem::path) */
    static Pointer New(volcart::filesystem::path path);

    /** @overload Volume(volcart::filesystem::path, Identifier, std::string) */
    static Pointer New(
        volcart::filesystem::path path, Identifier uuid, std::string name);
    /**@}*/
};
}  // namespace volcart
