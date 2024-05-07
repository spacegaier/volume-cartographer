#include "vc/core/types/VolumeZarr.hpp"

namespace fs = volcart::filesystem;

using namespace volcart;

// Load a Volume from disk
VolumeZarr::VolumeZarr(fs::path path) : Volume(path)
{
    if (metadata_.get<std::string>("type") != "zarr") {
        throw std::runtime_error("File not of type: zarr");
    }

    width_ = metadata_.get<int>("width");
    height_ = metadata_.get<int>("height");
    slices_ = metadata_.get<int>("slices");
    numSliceCharacters_ = std::to_string(slices_).size();

    std::vector<std::mutex> init_mutexes(slices_);

    slice_mutexes_.swap(init_mutexes);
}

// Setup a Volume from a folder of slices
VolumeZarr::VolumeZarr(fs::path path, std::string uuid, std::string name)
    : Volume(path, uuid, name)
{
    metadata_.set("type", "vol");
    metadata_.set("width", width_);
    metadata_.set("height", height_);
    metadata_.set("slices", slices_);
    metadata_.set("voxelsize", double{});
    metadata_.set("min", double{});
    metadata_.set("max", double{});
}

// Load a Volume from disk, return a pointer
VolumeZarr::Pointer VolumeZarr::New(fs::path path)
{
    return std::make_shared<VolumeZarr>(path);
}

// Set a Volume from a folder of slices, return a pointer
VolumeZarr::Pointer VolumeZarr::New(fs::path path, std::string uuid, std::string name)
{
    return std::make_shared<VolumeZarr>(path, uuid, name);
}