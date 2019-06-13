cmake_minimum_required(VERSION 3.2)

aux_source_directory(${MEMORYPROFILE_CRAWLER_DIR}/Crawler CRAWLER_DIR_SRCS)

#set(SOURCE_FILES main.cpp record.cpp ${CRAWLER_DIR_SRCS})

add_executable(UnityProfiler main.cpp Crawler/record.cpp ${CRAWLER_DIR_SRCS})

target_include_directories(UnityProfiler PUBLIC ${MEMORYPROFILE_INSTALL_INCLUDE_DIR} ${MEMORYPROFILE_CRAWLER_DIR}  ${MEMORYPROFILE_CRAWLER_DIR}/Crawler)

find_library(LIB_SQLLITE sqlite3)

target_link_libraries(UnityProfiler ${LIB_SQLLITE})

install(TARGETS UnityProfiler DESTINATION ${MEMORYPROFILE_INSTALL_BIN_DIR})