cmake_minimum_required(VERSION 3.2)
#project(MemoryCrawler)

aux_source_directory(Crawler CRAWLER_DIR_SRCS)

#set(SOURCE_FILES main.cpp ${CRAWLER_DIR_SRCS})

add_executable(MemoryCrawler main.cpp ${CRAWLER_DIR_SRCS})

#target_include_directories(MemoryCrawler PUBLIC ${MEMORYPROFILE_INSTALL_INCLUDE_DIR})

find_library(LIB_SQLLITE sqlite3)

target_link_libraries(MemoryCrawler ${LIB_SQLLITE})

install(TARGETS MemoryCrawler DESTINATION ${MEMORYPROFILE_INSTALL_BIN_DIR})
