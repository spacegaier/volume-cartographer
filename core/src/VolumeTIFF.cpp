#include "vc/core/types/VolumeTIFF.hpp"

#include "vc/core/io/TIFFIO.hpp"

#include <opencv2/imgcodecs.hpp>

namespace fs = volcart::filesystem;
namespace tio = volcart::tiffio;

using namespace volcart;

// Load a Volume from disk
VolumeTIFF::VolumeTIFF(fs::path path) : Volume(std::move(path))
{
    format_ = TIFF;

    width_ = metadata_.get<int>("width");
    height_ = metadata_.get<int>("height");
    slices_ = metadata_.get<int>("slices");
    numSliceCharacters_ = std::to_string(slices_).size();

    std::vector<std::mutex> init_mutexes(slices_);

    slice_mutexes_.swap(init_mutexes);
}

// Setup a Volume from a folder of slices
VolumeTIFF::VolumeTIFF(fs::path path, std::string uuid, std::string name)
    : Volume(std::move(path), std::move(uuid), std::move(name))
{
    format_ = TIFF;
    metadata_.set("type", "vol");
    metadata_.set("width", width_);
    metadata_.set("height", height_);
    metadata_.set("slices", slices_);
    metadata_.set("voxelsize", double{});
    metadata_.set("min", double{});
    metadata_.set("max", double{});
}

auto VolumeTIFF::getSlicePath(int index) const -> fs::path
{
    std::stringstream ss;
    ss << std::setw(numSliceCharacters_) << std::setfill('0') << index
       << ".tif";
    return path_ / ss.str();
}

void VolumeTIFF::setSliceData(int index, const cv::Mat& slice, bool compress)
{
    auto slicePath = getSlicePath(index);
    tio::WriteTIFF(
        slicePath.string(), slice,
        (compress) ? tiffio::Compression::LZW : tiffio::Compression::NONE);
}

auto VolumeTIFF::load_slice_(int index, VolumeAxis axis) const -> cv::Mat
{
    {
        std::unique_lock<std::shared_mutex> lock(print_mutex_);
        std::cout << "Requested to load slice " << index << std::endl;
    }

    auto slicePath = getSlicePath(index);
    return cv::imread(slicePath.string(), -1);
}

auto VolumeTIFF::cache_slice_(int index) const -> cv::Mat 
{
    // Check if the slice is in the cache.
    {
        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        if (cache_->contains(index)) {
            return cache_->get(index);
        }
    }

    {
        // Get the lock for this slice.
        auto& mutex = slice_mutexes_[index];

        // If the slice is not in the cache, get exclusive access to this slice's mutex.
        std::unique_lock<std::mutex> lock(mutex);
        // Check again to ensure the slice has not been added to the cache while waiting for the lock.
        {
            std::shared_lock<std::shared_mutex> lock(cache_mutex_);
            if (cache_->contains(index)) {
                return cache_->get(index);
            }
        }
        // Load the slice and add it to the cache.
        {
            auto slice = load_slice_(index);
            std::unique_lock<std::shared_mutex> lock(cache_mutex_);
            cache_->put(index, slice);
            return slice;
        }
    }

}