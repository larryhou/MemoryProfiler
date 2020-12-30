aux_source_directory(. SRC_FILES)

file(GLOB PROJ_HEADERS "*.h" "./Crawler/*.h")

source_group("Headers" FILES ${PROJ_HEADERS})

# set output path
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin")

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
    md5
)
