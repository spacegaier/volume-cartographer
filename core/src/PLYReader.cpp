#include "vc/core/io/PLYReader.hpp"

#include <cstdint>

#include "vc/core/types/Exceptions.hpp"
#include "vc/core/util/Logging.hpp"
#include "vc/core/util/String.hpp"

using namespace volcart;
using namespace volcart::io;
namespace fs = volcart::filesystem;

auto PLYReader::read() -> ITKMesh::Pointer
{
    std::setlocale(LC_ALL, "C");

    if (inputPath_.empty() || !fs::exists(inputPath_)) {
        auto msg = "File not provided or does not exist.";
        throw volcart::IOException(msg);
    }

    // Resets values of member variables in case of 2nd reading
    pointList_.clear();
    faceList_.clear();
    properties_.clear();
    vertextByteInfo_.clear();
    elementsList_.clear();
    outMesh_ = ITKMesh::New();
    numVertices_ = 0;
    numFaces_ = 0;
    hasLeadingChar_ = true;
    hasPointNorm_ = false;

    int skippedElementCnt = 0;

    plyFile_.open(inputPath_.string());
    if (!plyFile_.is_open()) {
        auto msg = "Open file " + inputPath_.string() + " failed.";
        throw volcart::IOException(msg);
    }
    parse_header_();
    for (auto& cur : elementsList_) {
        if (cur == "vertex") {
            read_points_();
        } else if (cur == "face") {
            read_faces_();
        } else {
            int curSkip = skippedLine_[skippedElementCnt];
            for (int i = 0; i < curSkip; i++) {
                std::getline(plyFile_, line_);
            }
            skippedElementCnt++;
        }
    }
    plyFile_.close();
    create_mesh_();

    return outMesh_;
}

void PLYReader::parse_header_()
{
    std::getline(plyFile_, line_);
    while (line_ != "end_header") {
        if (line_.find("format") != std::string::npos) {
            auto splitLine = split(line_, ' ');
            if (splitLine[1] == "ascii") {
                format_ = ASCII;
            } else if (splitLine[1] == "binary_little_endian") {
                format_ = BINARY_LITTLE_ENDIAN;
            } else if (splitLine[1] == "binary_big_endian") {
                format_ = BINARY_BIG_ENDIAN;
            }
            std::getline(plyFile_, line_);
        } else if (line_.find("element") != std::string::npos) {
            auto splitLine = split(line_, ' ');
            elementsList_.push_back(splitLine[1]);
            bool processVertex = false;

            if (splitLine[1] == "vertex") {
                numVertices_ = std::stoi(splitLine[2]);
                processVertex = true;
            } else if (splitLine[1] == "face") {
                numFaces_ = std::stoi(splitLine[2]);
            } else {
                skippedLine_.push_back(std::stoi(splitLine[2]));
            }
            std::getline(plyFile_, line_);
            splitLine = split(line_, ' ');
            int currentLine{0};
            while (splitLine[0] == "property") {
                if (splitLine[1] == "list") {
                    hasLeadingChar_ = line_.find("uchar") != std::string::npos;
                }
                // Not sure how to handle if it's not the vertices or faces
                else {
                    if (splitLine[2] == "nx") {
                        hasPointNorm_ = true;
                    }
                    properties_[splitLine[2]] = currentLine;

                    if (processVertex) {
                        unsigned int length;
                        if (splitLine[1] == "double" || splitLine[1] == "float64") {
                            length = 8;
                        } else if (splitLine[1] == "int"   || splitLine[1] == "int32"   ||
                                   splitLine[1] == "uint"  || splitLine[1] == "unint32" || 
                                   splitLine[1] == "float" || splitLine[1] == "float32") {
                            length = 4;
                        } else if (splitLine[1] == "short" || splitLine[1] == "ushort" ||
                                   splitLine[1] == "int16" || splitLine[1] == "uint16") {
                            length = 2;
                        } else if (splitLine[1] == "char" || splitLine[1] == "uchar" ||
                                   splitLine[1] == "int8" || splitLine[1] == "uint8") {
                            length = 1;
                        }

                        vertextByteInfo_[splitLine[2]] = {length, vertextByteLength_};
                        vertextByteLength_ += length;
                    }
                }
                std::getline(plyFile_, line_);
                currentLine++;
                splitLine = split(line_, ' ');
            }
        } else {
            std::getline(plyFile_, line_);
        }
    }
    if (numFaces_ == 0) {
        Logger()->warn("Warning: No face information found");
    }
}  // ParseHeader

auto convert(char* buffer, int length) -> double
{
    double returnDouble;
    float returnFloat;
    int returnInt;
    char returnChar;

    if (length == sizeof(double)) {
        memcpy(&returnDouble, buffer, length);
        return returnDouble;
    } else if (length == sizeof(float)) {
        memcpy(&returnFloat, buffer, length);
        return static_cast<double>(returnFloat);
    } else if (length == sizeof(int)) {
        memcpy(&returnInt, buffer, length);
        return static_cast<double>(returnInt);
    } else if (length == sizeof(char)) {
        memcpy(&returnChar, buffer, length);
        return static_cast<double>(returnChar);
    } else {
        auto msg = "Could not convert byte segment";
        throw volcart::IOException(msg);
    }
}

void PLYReader::read_points_()
{
    char buffer[vertextByteLength_];

    for (int i = 0; i < numVertices_; i++) {
        SimpleMesh::Vertex curPoint;

        if (format_ == ASCII) {
            std::getline(plyFile_, line_);
            auto curLine = split(line_, ' ');
            curPoint.x = std::stod(curLine[properties_["x"]]);
            curPoint.y = std::stod(curLine[properties_["y"]]);
            curPoint.z = std::stod(curLine[properties_["z"]]);
            if (properties_.find("nx") != properties_.end()) {
                curPoint.nx = std::stod(curLine[properties_["nx"]]);
                curPoint.ny = std::stod(curLine[properties_["ny"]]);
                curPoint.nz = std::stod(curLine[properties_["nz"]]);
            }
            if (properties_.find("red") != properties_.end()) {
                curPoint.r = std::stoi(curLine[properties_["red"]]);
                curPoint.g = std::stoi(curLine[properties_["green"]]);
                curPoint.b = std::stoi(curLine[properties_["blue"]]);
            }
        } else {            
            plyFile_.read(buffer, vertextByteLength_);
            char bufferX[vertextByteInfo_["x"].length];
            char bufferY[vertextByteInfo_["y"].length];
            char bufferZ[vertextByteInfo_["z"].length];

            memcpy(&bufferX, buffer + vertextByteInfo_["x"].offset, vertextByteInfo_["x"].length);
            curPoint.x = convert(bufferX, vertextByteInfo_["x"].length);            
            memcpy(&bufferY, buffer + vertextByteInfo_["y"].offset, vertextByteInfo_["y"].length);
            curPoint.y = convert(bufferY, vertextByteInfo_["y"].length);     
            memcpy(&bufferZ, buffer + vertextByteInfo_["z"].offset, vertextByteInfo_["z"].length);
            curPoint.z = convert(bufferZ, vertextByteInfo_["z"].length);     

            if (properties_.find("nx") != properties_.end()) {
                char bufferNX[vertextByteInfo_["nx"].length];
                char bufferNY[vertextByteInfo_["ny"].length];
                char bufferNZ[vertextByteInfo_["nz"].length];
                memcpy(&bufferNX, buffer + vertextByteInfo_["nx"].offset, vertextByteInfo_["nx"].length);
                curPoint.nx = convert(bufferNX, vertextByteInfo_["nx"].length);
                memcpy(&bufferNY, buffer + vertextByteInfo_["ny"].offset, vertextByteInfo_["ny"].length);
                curPoint.ny = convert(bufferNY, vertextByteInfo_["ny"].length);
                memcpy(&bufferNZ, buffer + vertextByteInfo_["nz"].offset, vertextByteInfo_["nz"].length);
                curPoint.nz = convert(bufferNZ, vertextByteInfo_["nz"].length);
            }
            if (properties_.find("red") != properties_.end()) {
                char bufferRed[vertextByteInfo_["red"].length];
                char bufferGreen[vertextByteInfo_["green"].length];
                char bufferBlue[vertextByteInfo_["blue"].length];
                memcpy(&bufferRed,   buffer + vertextByteInfo_["red"].offset,   vertextByteInfo_["red"].length);
                curPoint.r = convert(bufferRed,   vertextByteInfo_["red"].length);
                memcpy(&bufferGreen, buffer + vertextByteInfo_["green"].offset, vertextByteInfo_["green"].length);
                curPoint.g = convert(bufferGreen, vertextByteInfo_["green"].length);
                memcpy(&bufferBlue,  buffer + vertextByteInfo_["blue"].offset,  vertextByteInfo_["blue"].length);
                curPoint.b = convert(bufferBlue,  vertextByteInfo_["blue"].length);
            }
        }
        pointList_.push_back(curPoint);  
    }
}

void PLYReader::read_faces_()
{
    if (format_ != ASCII) {
        // Binary face reading is not yet supported
        return;
    }

    for (int i = 0; i < numFaces_; i++) {
        SimpleMesh::Cell face;
        std::getline(plyFile_, line_);
        auto curFace = split(line_, ' ');
        if (hasLeadingChar_) {
            int pointsPerFace = std::stoi(curFace[0]);
            if (pointsPerFace != 3) {
                auto msg = "Not a Triangular Mesh";
                throw volcart::IOException(msg);
            } else {
                face = SimpleMesh::Cell(
                    std::stoul(curFace[1]), std::stoul(curFace[2]),
                    std::stoul(curFace[3]));
                faceList_.push_back(face);
            }
        } else {
            if (curFace.size() != 3) {
                auto msg = "Not a Triangular Mesh";
                throw volcart::IOException(msg);
            } else {
                face = SimpleMesh::Cell(
                    std::stoul(curFace[1]), std::stoul(curFace[2]),
                    std::stoul(curFace[3]));
                faceList_.push_back(face);
            }
        }
    }
}

void PLYReader::create_mesh_()
{
    ITKPoint p;
    std::uint32_t pointCount = 0;
    for (auto& cur : pointList_) {
        p[0] = cur.x;
        p[1] = cur.y;
        p[2] = cur.z;
        outMesh_->SetPoint(pointCount, p);
        if (hasPointNorm_) {
            ITKPixel q;
            q[0] = cur.nx;
            q[1] = cur.ny;
            q[2] = cur.nz;
            outMesh_->SetPointData(pointCount, q);
        }
        pointCount++;
    }
    std::uint32_t faceCount = 0;
    for (auto& cur : faceList_) {
        ITKCell::CellAutoPointer cellpointer;
        cellpointer.TakeOwnership(new ITKTriangle);

        cellpointer->SetPointId(0, cur.v1);
        cellpointer->SetPointId(1, cur.v2);
        cellpointer->SetPointId(2, cur.v3);
        outMesh_->SetCell(faceCount, cellpointer);
        faceCount++;
    }
}

PLYReader::PLYReader(fs::path path) : inputPath_{std::move(path)} {}

void PLYReader::setPath(fs::path path) { inputPath_ = std::move(path); }

auto PLYReader::getMesh() -> ITKMesh::Pointer { return outMesh_; }
