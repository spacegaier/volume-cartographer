option(VC_BUILD_JSON "Build in-source JSON library" on)
if(VC_BUILD_JSON)
    FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/archive/v3.10.2.tar.gz
    )

    FetchContent_GetProperties(json)
    if(NOT json_POPULATED)
        set(JSON_BuildTests OFF CACHE INTERNAL "")
        set(JSON_Install ON CACHE INTERNAL "")
        FetchContent_Populate(json)
        add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
else()
    find_package(nlohmann_json 3.9.1 REQUIRED)
endif()