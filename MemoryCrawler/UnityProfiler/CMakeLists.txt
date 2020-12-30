aux_source_directory(. SRC_FILES)

file(GLOB PROJ_HEADERS "*.h")

source_group("Headers" FILES ${PROJ_HEADERS})

# set output path
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin")

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
