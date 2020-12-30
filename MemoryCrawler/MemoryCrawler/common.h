//
//  common.h
//  MemoryCrawler
//
//  Created by rayn on 2020/12/29.
//  Copyright © 2020 rayn. All rights reserved.
//

#ifndef common_h
#define common_h

/*
 * Platform macro
 */
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    #define MEMORY_CRAWLER_WINDOWS 1
#elif __APPLE__
    #define MEMORY_CRAWLER_MACOS 1
#elif __linux__
    #define MEMORY_CRAWLER_LINUX 1
#elif __unix__
    #define MEMORY_CRAWLER_UNIX 1
#endif


#ifdef MEMORY_CRAWLER_WINDOWS
    #include <direct.h>
    #include <functional>
#else
    #include <sys/stat.h>
    #include <unistd.h>
#endif


#ifdef MEMORY_CRAWLER_WINDOWS
    #define MKDIR(a, b) _mkdir(a)
    
    /* create a shared pointer for char array */
    #define NEW_CHAR_ARR(varname, count) std::shared_ptr<char[]> varname(new char[_count])
    #define GET_CHAR_ARR_PTR(varname) varname.get()
#else
    #define MKDIR(a, b) mkdir(a, b)
    
    #define NEW_CHAR_ARR(varname, count) char varname[count]
    #define GET_CHAR_ARR_PTR(varname) varname
#endif

#endif
