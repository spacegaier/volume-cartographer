#include "vc/core/types/Volume.hpp"
#include "vc/core/types/VolumeTIFF.hpp"
#include "vc/core/types/VolumeZARR.hpp"

#include <iomanip>
#include <sstream>

namespace fs = volcart::filesystem;

using namespace volcart;

// Load a Volume from disk
Volume::Volume(fs::path path) : DiskBasedObjectBaseClass(std::move(path))
{
    if (metadata_.get<std::string>("type") != "vol") {
        throw std::runtime_error("File not of type: vol");
    }
}

// Setup a Volume from a folder of slices
Volume::Volume(fs::path path, std::string uuid, std::string name)
    : DiskBasedObjectBaseClass(
        std::move(path), std::move(uuid), std::move(name)),
        slice_mutexes_(slices_)
{
}

// Load a Volume from disk, return a pointer
auto Volume::New(fs::path path) -> Volume::Pointer
{
    std::string format;

    try {
        format = volcart::Metadata(path / DiskBasedObjectBaseClass::METADATA_FILE).get<std::string>("format");
    } catch (std::runtime_error) {
        // Key "format" not found => continue and fallback to TIFF
    }

    if (format == "zarr") {
        return std::make_shared<VolumeZARR>(path);
    } else {
        return std::make_shared<VolumeTIFF>(path);
    }
}

// Set a Volume from a folder of slices, return a pointer
auto Volume::New(fs::path path, std::string uuid, std::string name)
    -> Volume::Pointer
{
    std::string format;

    try {
        format = volcart::Metadata(path / DiskBasedObjectBaseClass::METADATA_FILE).get<std::string>("format");
    } catch (std::runtime_error) {
        // Key "format" not found => continue and fallback to TIFF
    }

    if (format == "zarr") {
        return std::make_shared<VolumeZARR>(path, uuid, name);
    } else {
        return std::make_shared<VolumeTIFF>(path, uuid, name);
    }    
}

auto Volume::sliceWidth() const -> int { return width_; }
auto Volume::sliceHeight() const -> int { return height_; }
auto Volume::numSlices() const -> int { return slices_; }
auto Volume::voxelSize() const -> double
{
    return metadata_.get<double>("voxelsize");
}
auto Volume::min() const -> double { return metadata_.get<double>("min"); }
auto Volume::max() const -> double { return metadata_.get<double>("max"); }
auto Volume::format() const -> VolumeFormat { return format_; }

void Volume::setSliceWidth(int w)
{
    width_ = w;
    metadata_.set("width", w);
}

void Volume::setSliceHeight(int h)
{
    height_ = h;
    metadata_.set("height", h);
}

void Volume::setNumberOfSlices(std::size_t numSlices)
{
    slices_ = numSlices;
    numSliceCharacters_ = std::to_string(numSlices).size();
    metadata_.set("slices", numSlices);
}

void Volume::setVoxelSize(double s) { metadata_.set("voxelsize", s); }
void Volume::setMin(double m) { metadata_.set("min", m); }
void Volume::setMax(double m) { metadata_.set("max", m); }

auto Volume::bounds() const -> Volume::Bounds
{
    return {
        {0, 0, 0},
        {static_cast<double>(width_), static_cast<double>(height_),
         static_cast<double>(slices_)}};
}

auto Volume::isInBounds(double x, double y, double z) const -> bool
{
    return x >= 0 && x < width_ && y >= 0 && y < height_ && z >= 0 &&
           z < slices_;
}

auto Volume::isInBounds(const cv::Vec3d& v) const -> bool
{
    return isInBounds(v(0), v(1), v(2));
}

auto Volume::getSliceDataDefault(int index, cv::Rect rect, VolumeAxis axis) const -> cv::Mat
{
    // ToDo use "isChunkedFormat" variable
    if (format_ == ZARR) {
        return getSliceDataRect(index, rect, axis);
    } else {
        return getSliceData(index, axis);
    }
}

auto Volume::getSliceDataCopy(int index, VolumeAxis axis) const -> cv::Mat
{
    return getSliceData(index, axis).clone();
}

auto Volume::getSliceDataRectCopy(int index, cv::Rect rect, VolumeAxis axis) const -> cv::Mat
{
    return getSliceDataRect(index, rect, axis).clone();
}

auto Volume::intensityAt(int x, int y, int z) const -> std::uint16_t
{
    // clang-format off
    if (x < 0 || x >= sliceWidth() ||
        y < 0 || y >= sliceHeight() ||
        z < 0 || z >= numSlices()) {
        return 0;
    }
    // clang-format on
    return getSliceData(z).at<std::uint16_t>(y, x);
}

// Trilinear Interpolation
// From: https://en.wikipedia.org/wiki/Trilinear_interpolation
auto Volume::interpolateAt(double x, double y, double z) const -> std::uint16_t
{
    // insert safety net
    if (!isInBounds(x, y, z)) {
        return 0;
    }

    double intPart;
    double dx = std::modf(x, &intPart);
    auto x0 = static_cast<int>(intPart);
    int x1 = x0 + 1;
    double dy = std::modf(y, &intPart);
    auto y0 = static_cast<int>(intPart);
    int y1 = y0 + 1;
    double dz = std::modf(z, &intPart);
    auto z0 = static_cast<int>(intPart);
    int z1 = z0 + 1;

    auto c00 =
        intensityAt(x0, y0, z0) * (1 - dx) + intensityAt(x1, y0, z0) * dx;
    auto c10 =
        intensityAt(x0, y1, z0) * (1 - dx) + intensityAt(x1, y0, z0) * dx;
    auto c01 =
        intensityAt(x0, y0, z1) * (1 - dx) + intensityAt(x1, y0, z1) * dx;
    auto c11 =
        intensityAt(x0, y1, z1) * (1 - dx) + intensityAt(x1, y1, z1) * dx;

    auto c0 = c00 * (1 - dy) + c10 * dy;
    auto c1 = c01 * (1 - dy) + c11 * dy;

    auto c = c0 * (1 - dz) + c1 * dz;
    return static_cast<std::uint16_t>(cvRound(c));
}

auto Volume::reslice(
    const cv::Vec3d& center,
    const cv::Vec3d& xvec,
    const cv::Vec3d& yvec,
    int width,
    int height) const -> Reslice
{
    auto xnorm = cv::normalize(xvec);
    auto ynorm = cv::normalize(yvec);
    auto origin = center - ((width / 2) * xnorm + (height / 2) * ynorm);

    cv::Mat m(height, width, CV_16UC1);
    for (int h = 0; h < height; ++h) {
        for (int w = 0; w < width; ++w) {
            m.at<std::uint16_t>(h, w) =
                interpolateAt(origin + (h * ynorm) + (w * xnorm));
        }
    }

    return Reslice(m, origin, xnorm, ynorm);
}

void Volume::cachePurge() const 
{
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    cache_->purge();
}