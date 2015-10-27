#ifndef _VC_METADATA_H_
#define _VC_METADATA_H_

#include <iostream>
#include <fstream>

#include "../picojson.h"

#include "../vc_defines.h"

namespace volcart {
    class Metadata {
    public:
        Metadata();
        Metadata(std::string file_location);

        void save(std::string);

        // Debug functions
        void printString();
        void printObject();

        // Retrieval
        int getInt(std::string);
        double getDouble(std::string);
        std::string getString(std::string, std::string);

        // Assignment
        void setValue(std::string, int);
        void setValue(std::string, double);
        void setValue(std::string, std::string);

    protected:
        picojson::value _json;
    };
}

#endif // _VC_METADATA_H_
