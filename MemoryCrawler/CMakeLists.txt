cmake_minimum_required(VERSION 3.1)

include( CheckCXXCompilerFlag )
check_cxx_compiler_flag( "-std=c++11"   COMPILER_SUPPORTS_CXX11 )
check_cxx_compiler_flag( "-std=c++0x"   COMPILER_SUPPORTS_CXX0X )

if( COMPILER_SUPPORTS_CXX11 )
    if( CMAKE_COMPILER_IS_GNUCXX )
        set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11" )
    else()
        set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )
    endif()
elseif( COMPILER_SUPPORTS_CXX0X)
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x" )
else()
    # MSVC, On by default (if available)
endif()

set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})

set(MEMORYPROFILE_INSTALL_BIN_DIR ${PROJECT_SOURCE_DIR}/bin)
set(MEMORYPROFILE_INSTALL_LIB_DIR ${PROJECT_SOURCE_DIR}/lib)
set(MEMORYPROFILE_CRAWLER_DIR ${PROJECT_SOURCE_DIR}/MemoryCrawler)

add_subdirectory(MemoryCrawler)
add_subdirectory(UnityProfiler)