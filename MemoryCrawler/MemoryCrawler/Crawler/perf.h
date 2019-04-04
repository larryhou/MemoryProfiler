//
//  perf.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/4.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef perf_h
#define perf_h

#include <iostream>
#include <map>
#include <vector>
#include <chrono>
#include "types.h"

using std::vector;
using std::string;
using std::map;
using std::pair;

using std::chrono::high_resolution_clock;
using clocktime_t = std::chrono::time_point<high_resolution_clock>;

using std::cout;


class TimeSampler
{
    vector<const char *> __events;
    
    map<int, clocktime_t> __timestamps;
    map<int, clocktime_t> __records;
    
    vector<int> __entities;
    map<int, int> __bridges;
    vector<int> __cursors;
    
public:
    int begin(const char *event);
    int64_t end();
    
    void summary();
    
private:
    clocktime_t clock()
    {
        return high_resolution_clock::now();
    }
    
    int64_t duration(clocktime_t time, clocktime_t base)
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(time - base).count();
    }
    
    void dump(map<int, vector<int>> &connections, int index, const char *indent);
};

#endif /* perf_h */
