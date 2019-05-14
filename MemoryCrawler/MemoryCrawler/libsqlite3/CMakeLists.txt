aux_source_directory(. SRC_FILES)

file(GLOB PROJ_HEADERS "*.h")

source_group("Headers" FILES ${PROJ_HEADERS})

add_library(
    Sqlite3
    ${SRC_FILES}
    ${PROJ_HEADERS}
)
