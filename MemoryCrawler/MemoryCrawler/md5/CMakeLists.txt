aux_source_directory(. SRC_FILES)

file(GLOB PROJ_HEADERS "*.h")

source_group("Headers" FILES ${PROJ_HEADERS})

add_library(
    md5
    ${SRC_FILES}
    ${PROJ_HEADERS}
)