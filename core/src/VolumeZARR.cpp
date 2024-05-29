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

auto VolumeZARR::getSliceData(int index, VolumeAxis axis) const -> cv::Mat
{       
    // Not implemented yet}
    return cv::Mat();
}

auto VolumeZARR::getSliceDataRect(int index, cv::Rect rect, VolumeAxis axis) const -> cv::Mat
{
    return load_slice_(index, rect, axis);
}

void VolumeZARR::setSliceData(int index, const cv::Mat& slice, bool compress)
{
    // Not implemented yet
}

void VolumeZARR::setZarrLevel(int level) { zarrLevel_ = level; }

auto VolumeZARR::zarrLevels() const -> std::vector<std::string>
{ 
    std::vector<std::string> keys;
    zarrFile_.keys(keys);
    std::sort(keys.begin(), keys.end());
    return keys;
}

auto VolumeZARR::load_slice_(int index, cv::Rect2i rect, VolumeAxis axis) const -> cv::Mat
{
    {
        std::unique_lock<std::shared_mutex> lock(print_mutex_);
        std::cout << "Requested to load slice " << index << std::endl;
    }

    if (zarrDs_) {
        
        //getChunkSliceData(index, rect);
        xt::xtensor<std::uint16_t, 3>* data = nullptr;

        if (axis == Z) {
            // if (index == 0) {
            //     // Initially load only 1 slice to quickly show something to the user
            //     z5::types::ShapeType offset = {0, 0, 0};
            //     auto shape = zarrDs_->shape();
            //     shape.front() = 1;
            //     xt::xtensor<std::uint16_t, 3>::shape_type tensorShape;
            //     tensorShape = {shape[0], shape[1], shape[2]};
            //     data = new xt::xtensor<std::uint16_t, 3>(tensorShape);
            //     int threads = static_cast<int>(std::thread::hardware_concurrency());
            //     z5::multiarray::readSubarray<std::uint16_t>(*zarrDs_, *data, offset.begin(), threads);
            //     // std::cout << "Length: " << data->size() << std::endl;
            //     // std::cout << "Dimmension: " << data->dimension() << std::endl;
            //     // std::cout << "Shape Original: " << data->shape()[0] << ", " << data->shape()[1] << ", " << data->shape()[2] << std::endl;
            //     return cv::Mat(data->shape()[1], data->shape()[2], CV_16U, data->data(), 0);
            // } else {

                // z5::types::ShapeType chunkShape;
                // z5::types::ShapeType chunkIndex = {0, 0, 0};
                // zarrDs_->getChunkShape(chunkIndex, chunkShape);
                // // Determine required chunk number
                // size_t chunkNum = index / chunkShape[(int)axis];
                // size_t offset = chunkNum * chunkShape[(int)axis];

                // Adjust rect based on ZARR scaling
                // rect.x /= groupAttr_["multiscales"][0]["datasets"][zarrLevel_]["coordinateTransformations"][0]["scale"][0].get<float>();
                // rect.y /= groupAttr_["multiscales"][0]["datasets"][zarrLevel_]["coordinateTransformations"][0]["scale"][1].get<float>();
                // rect.width *= groupAttr_["multiscales"][0]["datasets"][zarrLevel_]["coordinateTransformations"][0]["scale"][0].get<float>();
                // rect.height *= groupAttr_["multiscales"][0]["datasets"][zarrLevel_]["coordinateTransformations"][0]["scale"][1].get<float>();

                xt::xtensor<std::uint16_t, 3>* data = nullptr;
                auto offsetShape = (axis == Z)   ? z5::types::ShapeType({(size_t)index,  (size_t)rect.y, (size_t)rect.x})
                                   : (axis == X) ? z5::types::ShapeType({(size_t)rect.x, (size_t)index,  (size_t)rect.y})
                                                 : z5::types::ShapeType({(size_t)rect.x, (size_t)rect.y, (size_t)index});
                // std::cout << "Offset Shape: " << offsetShape[0] << ", " << offsetShape[1] << ", " << offsetShape[2] << std::endl;

                // auto it = loadedChunks_[axis].find(chunkNum);
                // if (it == loadedChunks_[axis].end()) {
                    // auto chunkSize = res->getChunkSize(chunkIndex);
                    // std::uint16_t chunkData[chunkSize];
                    // res->readChunk(chunkIndex, chunkData);

                    // Prepare desired read shape to control how many slices get loaded
                    auto shape = zarrDs_->shape();
                    // shape.at((axis == Z) ? 0 : (axis == Y) ? 2 : 1) = chunkShape[(int)axis];  // load as many slices as there are in a single chunk
                    xt::xtensor<std::uint16_t, 3>::shape_type tensorShape;
                    size_t maxWidth = shape[2] - rect.x;
                    size_t maxHeight = shape.at(1) - rect.y;
                    tensorShape = {1, std::min((size_t)rect.height, maxHeight), std::min((size_t)rect.width, maxWidth)};
                    std::cout << "Tensor Shape: " << tensorShape[0] << ", " << tensorShape[1] << ", " << tensorShape[2] << std::endl;

                    // Read data
                    data = new xt::xtensor<std::uint16_t, 3>(tensorShape);
                    int threads = static_cast<int>(std::thread::hardware_concurrency());
                    zarrDs_->prepareMultiThreadedRead(threads);
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

                //     loadedChunks_[axis].emplace(chunkNum, data);
                // } else {
                //     data = it->second;
                // }

                //auto view = xt::view(*data, index % chunkShape[(int)axis]);
                // auto view = xt::view(*data, index % chunkShape[(int)axis]);
                // return cv::Mat(view.shape()[0], view.shape()[1], CV_16U, view.data() + view.data_offset(), 0);
                return cv::Mat(data->shape()[1], data->shape()[2], CV_16U, data->data(), 0);
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

            z5::filesystem::handle::Group group(fs::path(path_), z5::FileMode::FileMode::r);
            z5::readAttributes(group, groupAttr_);

            std::vector<std::mutex> init_mutexes(slices_);

            slice_mutexes_.swap(init_mutexes);
        }
    }
}

auto roundDownToNearestMultiple(float numToRound, int multiple)  ->int
{
    return ((static_cast<int>(numToRound) / multiple) * multiple);
}

void VolumeZARR::setChunkSettings(ChunkDataSettings chunkSettings)
{ 
    settings = chunkSettings; 

    // Hard-code for testing
    settings.offset = -125;
    settings.xAxis = 2;
    settings.yAxis = 0;
    settings.zAxis = 1;
    settings.scale = 4;
    settings.chunkSize = 25;
}

auto VolumeZARR::determineChunksForRect(int index, cv::Rect2i rect) const -> ChunkData
{
    // if (settings.path.size() == 0) {
    //     return {};
    // }

    ChunkData res;
    int axis = Z;

    auto offsetShape = (axis == Z)   ? z5::types::ShapeType({(size_t)index, (size_t)rect.y, (size_t)rect.x})
                       : (axis == X) ? z5::types::ShapeType({(size_t)rect.x, (size_t)index, (size_t)rect.y})
                                     : z5::types::ShapeType({(size_t)rect.x, (size_t)rect.y, (size_t)index});

    auto shape = zarrDs_->shape();
    xt::xtensor<std::uint16_t, 3>::shape_type tensorShape;
    size_t maxWidth = shape[2] - rect.x;
    size_t maxHeight = shape.at(1) - rect.y;
    tensorShape = {1, std::min((size_t)rect.height, maxHeight), std::min((size_t)rect.width, maxWidth)};
    std::cout << "Tensor Shape: " << tensorShape[0] << ", " << tensorShape[1] << ", " << tensorShape[2] << std::endl;

    std::vector<z5::types::ShapeType> chunkRequests;
    const auto& chunking = zarrDs_->chunking();
    chunking.getBlocksOverlappingRoi(offsetShape, shape, chunkRequests);

    for (auto chunkRequest : chunkRequests) {
        ChunkID id = {chunkRequest[0], chunkRequest[1], chunkRequest[2]};
        auto it = chunkData.find(id);
        std::cout << "Chunk ID: " << id[0] << "|" << id[1] << "|" << id[2] << std::endl;
        if (it != chunkData.end()) {
            res[id] = it->second;
        } else {
            res[id] = {};
        }
    }

    // auto xIndexStart = std::max(100, roundDownToNearestMultiple((rect.x - 100) / settings.scale, settings.chunkSize) - settings.offset);
    // xIndexStart -= settings.chunkSize; // due to the fact that file 000100 contains from -100 to 100, 000125 contains from 0 to 200, 000150 from 100 to 300
    // auto yIndexStart = std::max(100, roundDownToNearestMultiple((rect.y - 100) / settings.scale, settings.chunkSize) - settings.offset);
    // yIndexStart -= settings.chunkSize;

    // auto zIndexEnd = std::max(100, roundDownToNearestMultiple((index - 100) / settings.scale, settings.chunkSize) - settings.offset);
    // auto zIndexStart = zIndexEnd - settings.chunkSize;

    // auto xIndexEnd = roundDownToNearestMultiple((rect.x + rect.width - 100) / settings.scale, settings.chunkSize) - settings.offset;
    // auto yIndexEnd = roundDownToNearestMultiple((rect.y + rect.height - 100) / settings.scale, settings.chunkSize) - settings.offset;

    // ChunkID id;
    // for (auto z = zIndexStart; z <= zIndexEnd; z += settings.chunkSize) {
    //     for (auto x = xIndexStart; x <= xIndexEnd; x += settings.chunkSize) {
    //         for (auto y = yIndexStart; y <= yIndexEnd; y += settings.chunkSize) {
    //             id[settings.xAxis] = x;
    //             id[settings.yAxis] = y;
    //             id[settings.zAxis] = z;

    //             auto it = chunkData.find(id);
    //             if (it != chunkData.end()) {
    //                 res[id] = it->second;
    //             } else {
    //                 res[id] = {};
    //             }
    //         }
    //     }
    // }

    return res;
}

auto VolumeZARR::determineNotLoadedChunks(int index, cv::Rect2i rect) const -> ChunkRequests
{
    auto chunks = determineChunksForRect(index, rect);

    ChunkRequests requests;
    for(auto chunk : chunks) {
        if (chunk.second->size() == 0) {
            requests.push_back(ChunkRequest({chunk.first[0], chunk.first[1], chunk.first[2]}));
        }
    }

    return requests;

    // std::string folder;
    // ChunkFiles fileList;
    // QDir overlayMainFolder(settings.path);
    // auto absPath = overlayMainFolder.absolutePath();

    // for (auto chunk : chunks) {
    //     if (chunkData.find(chunk.first) == chunkData.end()) {
    //         // TODO:Check if the settings logic for axis really works here
    //         folder = QStringLiteral("%1")
    //                      .arg(chunk.first[settings.yAxis], 6, 10, QLatin1Char('0'))
    //                      .append("_" + QStringLiteral("%1").arg(chunk.first[settings.zAxis], 6, 10, QLatin1Char('0')))
    //                      .append("_" + QStringLiteral("%1").arg(chunk.first[settings.xAxis], 6, 10, QLatin1Char('0')));

    //         QDir overlayFolder(absPath + QDir::separator() + folder);
    //         QStringList files = overlayFolder.entryList({"*.ply", "*.obj"}, QDir::NoDotAndDotDot | QDir::Files);

    //         for (auto file : files) {
    //             file = overlayFolder.path() + QDir::separator() + file;
    //             fileList[chunk.first].push_back(file);
    //         }
    //     }
    // }
    //
    // return fileList;
}

void VolumeZARR::loadChunkFiles(ChunkRequests chunksToLoad, VolumeAxis axis) const
{
    if (chunksToLoad.size() == 0 || !zarrDs_) {
        return;
    }

    z5::types::ShapeType chunkShape;
    z5::types::ShapeType chunkIndex = {0, 0, 0};
    zarrDs_->getChunkShape(chunkIndex, chunkShape);

    auto shape = zarrDs_->shape();
    shape.at((axis == Z) ? 0 : (axis == Y) ? 2 : 1) = chunkShape[(int)axis];  // load as many slices as there are in a single chunk
    xt::xtensor<std::uint16_t, 3>::shape_type tensorShape;
    tensorShape = {1, shape[1], shape[2]};
    // tensorShape = {shape[0], shape[1], shape[2]};
    auto data = new xt::xtensor<std::uint16_t, 3>(tensorShape);

    z5::multiarray::readSubarrayMultiThreaded<std::uint16_t>(*zarrDs_, *data, z5::types::ShapeType(), shape, chunksToLoad, std::thread::hardware_concurrency());


    for (auto chunk : chunksToLoad) {
        chunkData[{chunk[0], chunk[1], chunk[2]}] = data + shape[1] * shape[2];
    }

    // auto view = xt::view(*data, index % chunkShape[(int)axis]);
    // return cv::Mat(view.shape()[0], view.shape()[1], CV_16U, view.data() + view.data_offset(), 0);

    // // Convert to flat work list for threads
    // std::vector<ChunkID> chunks;
    // std::vector<std::string> fileNames;
    // for (auto chunk : chunksToLoad) {
    //     for (auto file : chunk.second) {
    //         chunks.push_back(chunk.first);
    //         fileNames.push_back(file);
    //     }
    // }

    // int numThreads = static_cast<int>(std::thread::hardware_concurrency()) - 2;
    // for (int f = 0; f <= fileNames.size(); f += numThreads + 1) {
    //     std::vector<std::thread> threads;

    //     for (int i = 0; i <= numThreads && (f + i) < fileNames.size(); ++i) {
    //         threads.emplace_back(&VolumeZARR::loadSingleChunkFile, this, fileNames.at(f + i), chunks.at(f + i), i);
    //     }

    //     for (auto& t : threads) {
    //         t.join();
    //     }

    //     // // Check if prefetching was stopped or slice index changed
    //     // if (stopPrefetching.load() || prefetchSliceIndex.load() != currentSliceIndex) {
    //     //     break;
    //     // }
    // }

    // for (auto threadDataSet : threadData) {
    //     if (threadDataSet.second.size() > 0) {
    //         mergeThreadData(threadDataSet.second);
    //     }
    // }
}

void VolumeZARR::loadSingleChunkFile(std::string file, ChunkID chunkID, int threadNum) const
{
    // vc::ITKMesh::Pointer mesh;
    // itk::Point<double, 3> point;

    // std::cout << file.toStdString() << std::endl;
    // if (file.endsWith(".ply")) {
    //     volcart::io::PLYReader reader(fs::path(file.toStdString()));
    //     reader.read();
    //     mesh = reader.getMesh();
    // } else if (file.endsWith(".obj")) {
    //     volcart::io::OBJReader reader;
    //     reader.setPath(file.toStdString());
    //     reader.read();
    //     mesh = reader.getMesh();
    // } else {
    //     return;
    // }

    // auto numPoints = mesh->GetNumberOfPoints();

    // for (std::uint64_t pnt_id = 0; pnt_id < numPoints; pnt_id++) {
    //     point = mesh->GetPoint(pnt_id);
    //     point[0] += settings.offset;
    //     point[1] += settings.offset;
    //     point[2] += settings.offset;
    //     point[0] *= settings.scale;
    //     point[1] *= settings.scale;
    //     point[2] *= settings.scale;

    //     if (point[settings.xAxis] >= 0 && point[settings.yAxis] >= 0 && point[settings.zAxis] >= 0) {
    //         // Just a type conversion for the point, so do not use the axis settings here
    //         threadData[threadNum][chunkID].push_back({point[0], point[1], point[2]});
    //     }
    // }
}

void VolumeZARR::mergeThreadData(ChunkData threadData) const
{
    // std::lock_guard<std::shared_mutex> lock(dataMutex);

    // for (auto it = threadData.begin(); it != threadData.end(); ++it) {
    //     std::pair<ChunkData::iterator, bool> ins = chunkData.insert(*it);
    //     if (!ins.second) {  
    //         // Map key already existed, so we have to merge the slice data
    //         Chunck* vec1 = &(it->second);
    //         Chunck* vec2 = &(ins.first->second);
    //         vec2->insert(vec2->end(), vec1->begin(), vec1->end());
    //     }
    // }
}

auto VolumeZARR::getChunkData(int index, cv::Rect2i rect, VolumeAxis axis) -> ChunkData
{
    ChunkData res;

    loadChunkFiles(determineNotLoadedChunks(index, rect), axis);
    auto chunks = determineChunksForRect(index, rect);

    for (auto chunk : chunks) {
        res[chunk.first] = chunkData[chunk.first];
    }

    return res;
}

auto VolumeZARR::getChunkSliceData(int index, cv::Rect2i rect) const -> ChunkSlice
{
    ChunkSlice res;

    // loadChunkFiles(determineNotLoadedChunks(index, rect));
    // auto chunks = determineChunksForRect(index, rect);

    // for (auto chunk : chunks) {
    //     for (auto point : chunkData[chunk.first]) {
    //         if (point[settings.zAxis] == index) {
    //             res.push_back({point[settings.xAxis], point[settings.yAxis]});
    //         }
    //     }
    // }

    return res;
}