option(VC_BUILD_Z5 "Build in-source Z5 ZARR library" on)

if(VC_BUILD_Z5)
    ### xtl ###
    FetchContent_Declare(
        xtl
        URL https://github.com/xtensor-stack/xtl/archive/refs/tags/0.7.7.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP ON
    )

    FetchContent_GetProperties(xtl)
    if(NOT xtl_POPULATED)
        FetchContent_Populate(xtl)
        add_subdirectory(${xtl_SOURCE_DIR} ${xtl_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()  
    
    ### xtensor ###
    set(xtl_DIR ${xtl_BINARY_DIR})
    FetchContent_Declare(
        xtensor
        URL https://github.com/xtensor-stack/xtensor/archive/refs/tags/0.25.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP ON
    )

    FetchContent_GetProperties(xtensor)
    if(NOT xtensor_POPULATED)
        FetchContent_Populate(xtensor)
        add_subdirectory(${xtensor_SOURCE_DIR} ${xtensor_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()

    ### z5 ###
    set(xtensor_DIR ${xtensor_BINARY_DIR})
    set(nlohmann_json_DIR ${json_BINARY_DIR})
    FetchContent_Declare(
        z5
        URL https://github.com/spacegaier/z5/archive/refs/heads/master.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP ON
    )

    FetchContent_GetProperties(z5)
    if(NOT z5_POPULATED)
        set(BUILD_Z5PY off)
        set(xsimd_FOUND off)
        FetchContent_Populate(z5)
        add_subdirectory(${z5_SOURCE_DIR} ${z5_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
endif()