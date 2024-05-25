#include "vc/core/types/VolumeZARR.hpp"

#include <thread>

#include "xtensor/xmanipulation.hpp"
#include "xtensor/xview.hpp"
#include "z5/factory.hxx"
#include "z5/multiarray/xtensor_access.hxx"
#include "z5/attributes.hxx"

namespace fs = volcart::filesystem;

using namespace volcart;

// Load a Volume from disk
VolumeZARR::VolumeZARR(fs::path path) : Volume(std::move(path)), zarrFile_(path)
{
    format_ = ZARR;
    zarrFile_ = z5::filesystem::handle::File(path_); 
}

// Setup a Volume from a folder of slices
VolumeZARR::VolumeZARR(fs::path path, std::string uuid, std::string name)
    : Volume(std::move(path), std::move(uuid), std::move(name)),
      zarrFile_(path)
{
    format_ = ZARR;
}

auto VolumeZARR::getSlicePath(int index) const -> fs::path
{
    // There is no single path for a chunked format
    return fs::path();
}

void VolumeZARR::setSliceData(int index, const cv::Mat& slice, bool compress)
{
    // Not implemented yet
}

void VolumeZARR::setZarrLevel(int level) { zarrLevel_ = level; openZarr(); }

auto VolumeZARR::zarrLevels() const -> std::vector<std::string>
{ 
    std::vector<std::string> keys;
    zarrFile_.keys(keys);
    std::sort(keys.begin(), keys.end());
    return keys;
}

auto VolumeZARR::load_slice_(int index, VolumeAxis axis) const -> cv::Mat
{
    {
        std::unique_lock<std::shared_mutex> lock(print_mutex_);
        std::cout << "Requested to load slice " << index << std::endl;
    }

    if (zarrDs_) {

        xt::xtensor<std::uint16_t, 3>* data = nullptr;

        if (axis == Z) {
            if (index == 0) {
                // Initially load only 1 slice to quickly show something to the user
                z5::types::ShapeType offset = {0, 0, 0};
                auto shape = zarrDs_->shape();
                shape.front() = 1;
                xt::xtensor<std::uint16_t, 3>::shape_type tensorShape;
                tensorShape = {shape[0], shape[1], shape[2]};
                data = new xt::xtensor<std::uint16_t, 3>(tensorShape);
                int threads = static_cast<int>(std::thread::hardware_concurrency());
                z5::multiarray::readSubarray<std::uint16_t>(*zarrDs_, *data, offset.begin(), threads);
                // std::cout << "Length: " << data->size() << std::endl;
                // std::cout << "Dimmension: " << data->dimension() << std::endl;
                // std::cout << "Shape Original: " << data->shape()[0] << ", " << data->shape()[1] << ", " << data->shape()[2] << std::endl;
                return cv::Mat(data->shape()[1], data->shape()[2], CV_16U, data->data(), 0);
            } else {

                z5::types::ShapeType chunkShape;
                z5::types::ShapeType chunkIndex = {0, 0, 0};
                zarrDs_->getChunkShape(chunkIndex, chunkShape);
                // Determine required chunk number
                unsigned int chunkNum = index / chunkShape[(int)axis];
                unsigned int offset = chunkNum * chunkShape[(int)axis];

                std::cout << "Chunk Num: " << chunkNum << std::endl;

                xt::xtensor<std::uint16_t, 3>* data = nullptr;
                auto offsetShape = (axis == Z) ? z5::types::ShapeType({offset, 0, 0}) : (axis == X) ? z5::types::ShapeType({0, offset, 0}) : z5::types::ShapeType({0, 0, offset});
                std::cout << "Offset Shape: " << offsetShape[0] << ", " << offsetShape[1] << ", " << offsetShape[2] << std::endl;

                auto it = loadedChunks_[axis].find(chunkNum);
                if (it == loadedChunks_[axis].end()) {
                    // auto chunkSize = res->getChunkSize(chunkIndex);
                    // std::uint16_t chunkData[chunkSize];
                    // res->readChunk(chunkIndex, chunkData);

                    // Prepare desired read shape to control how many slices get loaded
                    auto shape = zarrDs_->shape();
                    shape.at((axis == Z) ? 0 : (axis == Y) ? 2 : 1) = 1;  // chunkShape[(int)axis];  // load as many slices as there are in a single chunk
                    xt::xtensor<std::uint16_t, 3>::shape_type tensorShape;
                    tensorShape = {shape[0], shape[1], shape[2]};

                    // Read data
                    data = new xt::xtensor<std::uint16_t, 3>(tensorShape);
                    int threads = static_cast<int>(std::thread::hardware_concurrency());
                    z5::multiarray::readSubarray<std::uint16_t>(*zarrDs_, *data, offsetShape.begin(), threads);

                    // // Adjust axis
                    // if (axis == X) {
                    //     std::cout << "Length: " << data->size() << std::endl;
                    //     std::cout << "Dimmension: " << data->dimension() << std::endl;
                    //     std::cout << "Shape Original: " << data->shape()[0] << ", " << data->shape()[1] << ", " << data->shape()[2] << std::endl;
                    //     //*data = xt::view(xt::swapaxes(*data, 0, 1), 1);
                    //     std::cout << "Length: " << data->size() << std::endl;
                    //     std::cout << "Dimmension: " << data->dimension() << std::endl;
                    //     std::cout << "Shape Original: " << data->shape()[0] << ", " << data->shape()[1] << ", " << data->shape()[2] << std::endl;
                    //     return cv::Mat(data->shape()[0], data->shape()[2], CV_16U, data->data(), 0);
                    // } else if (axis == Y) {
                    //     std::cout << "Length: " << data->size() << std::endl;
                    //     std::cout << "Dimmension: " << data->dimension() << std::endl;
                    //     std::cout << "Shape Original: " << data->shape()[0] << ", " << data->shape()[1] << ", " << data->shape()[2] << std::endl;
                    //     *data = xt::swapaxes(*data, 1, 2);
                    //     *data = xt::swapaxes(*data, 0, 1);
                    //     std::cout << "Length: " << data->size() << std::endl;
                    //     std::cout << "Dimmension: " << data->dimension() << std::endl;
                    //     std::cout << "Shape Original: " << data->shape()[0] << ", " << data->shape()[1] << ", " << data->shape()[2] << std::endl;
                    //     *data = xt::view(*data, 1);
                    //     std::cout << "Length: " << data->size() << std::endl;
                    //     std::cout << "Dimmension: " << data->dimension() << std::endl;
                    //     std::cout << "Shape Original: " << data->shape()[0] << ", " << data->shape()[1] << ", " << data->shape()[2] << std::endl;
                    //     return cv::Mat(data->shape()[0], data->shape()[2], CV_16U, data->data(), 0);
                    // }

                    loadedChunks_[axis].emplace(chunkNum, data);
                } else {
                    data = it->second;
                }

                auto view = xt::view(*data, index % chunkShape[(int)axis]);
                return cv::Mat(view.shape()[0], view.shape()[1], CV_16U, view.data() + view.data_offset(), 0);
            }
        } else {
            xt::xarray<std::uint16_t>* data = nullptr;
            auto offset = (axis == X) ? z5::types::ShapeType({0, (std::size_t)index, 0}) : z5::types::ShapeType({0, 0, (std::size_t)index});
            auto shape = zarrDs_->shape();
            shape.at((axis == X) ? 1 : 2) = 1;
            data = new xt::xarray<std::uint16_t>(shape);
            int threads = static_cast<int>(std::thread::hardware_concurrency());
            z5::multiarray::readSubarray<std::uint16_t>(*zarrDs_, *data, offset.begin(), threads);
            if (axis == X) {
                *data = xt::view(xt::swapaxes(*data, 0, 1), 1);
            } else {
                *data = xt::swapaxes(*data, 1, 2);
                *data = xt::swapaxes(*data, 0, 1);
                *data = xt::view(*data, 1);
            }
            // std::cout << "Length: " << data->size() << std::endl;
            // std::cout << "Dimmension: " << data->dimension() << std::endl;
            // std::cout << "Shape Original: " << data->shape()[0] << ", " << data->shape()[1] << ", " << data->shape()[2] << std::endl;

            return cv::Mat(data->shape()[0], data->shape()[2], CV_16U, data->data(), 0);
        }
    }

    // No valid data set or axis => return empty
    return cv::Mat();
}

auto VolumeZARR::cache_slice_(int index) const -> cv::Mat 
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

void VolumeZARR::openZarr()
{
    if (!zarrDs_ && zarrLevel_ >= 0) 
    {
        // Check that we have a valid level (>= 0 and in keys list)
        std::vector<std::string> keys;
        zarrFile_.keys(keys);
        if (std::find(keys.begin(), keys.end(), std::to_string(zarrLevel_)) != keys.end())
        {
            z5::filesystem::handle::Dataset hndl(
                fs::path(path_ / std::to_string(zarrLevel_)), z5::FileMode::FileMode::r);
            zarrDs_ = z5::filesystem::openDataset(hndl);

            width_ = zarrDs_->shape()[1];
            height_ = zarrDs_->shape()[2];
            slices_ = zarrDs_->shape()[0];

            std::vector<std::mutex> init_mutexes(slices_);

            slice_mutexes_.swap(init_mutexes);
        }
    }
}