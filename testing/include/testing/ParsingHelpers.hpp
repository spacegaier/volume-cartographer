//
// Created by Ryan Taber on 11/19/15.
//
#pragma once

#include <boost/filesystem/path.hpp>

#include "core/vc_defines.hpp"

/*
 * Purpose of File:
 *     - provides helpful parsing methods that are used by multiple
 *       testing files when reading in mesh or point cloud data
 */

namespace volcart
{
namespace testing
{

class ParsingHelpers
{

public:
    static void ParsePLYFile(
        const boost::filesystem::path& filepath,
        std::vector<Vertex>& verts,
        std::vector<Cell>& faces);
    static void ParseOBJFile(
        const boost::filesystem::path& filepath,
        std::vector<Vertex>& points,
        std::vector<Cell>& cells);

private:
    static std::vector<std::string> SplitString(std::string input);
};
}
}