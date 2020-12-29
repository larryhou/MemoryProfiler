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
#ifdef WIN32
#define MEMORY_CRAWLER_WINDOWS 1
#else
#define MEMORY_CRAWLER_WINDOWS 0
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
#else
#define MKDIR(a, b) mkdir(a, b)
#endif

#endif
