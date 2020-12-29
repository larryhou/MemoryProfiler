aux_source_directory(. SRC_FILES)

file(GLOB PROJ_HEADERS "*.h")

source_group("Headers" FILES ${PROJ_HEADERS})

# set output path
if(WIN32 OR MSVC)
    set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin")
else()
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin/Debug")
    else(CMAKE_BUILD_TYPE MATCHES "Debug")
        set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin/Release")
    endif(CMAKE_BUILD_TYPE MATCHES "Debug")
endif()

add_executable(
    UnityProfiler
    ${SRC_FILES}
    ${PROJ_HEADERS}
)

target_link_libraries(
    UnityProfiler
    Sqlite3
    MemoryCrawler_Crawler
    MemoryCrawler_Lib
    UnityProfiler_Crawler
)
