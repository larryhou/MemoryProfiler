aux_source_directory(. SRC_FILES)

file(GLOB PROJ_HEADERS "*.h")

source_group("Headers" FILES ${PROJ_HEADERS})

# set output path
if (CMAKE_BUILD_TYPE MATCHES "Debug")
    set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin/Debug")
else(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin/Release")
endif(CMAKE_BUILD_TYPE MATCHES "Debug")

add_library(
    MemoryCrawler_Lib
    ${SRC_FILES}
    ${PROJ_HEADERS}
)

add_executable(
    MemoryCrawler
    ${SRC_FILES}
    ${PROJ_HEADERS}
)

target_link_libraries(
    MemoryCrawler
    Sqlite3
    MemoryCrawler_Crawler
)
