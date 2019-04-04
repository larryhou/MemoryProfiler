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

using std::chrono::system_clock;
using std::cout;


class TimeSampler
{
    vector<const char *> __events;
    
    map<int, int64_t> __timestamps;
    map<int, int64_t> __records;
    
    vector<int> __entities;
    map<int, int> __bridges;
    vector<int> __cursors;
    
public:
    int begin(const char *event);
    int64_t end();
    
    void summary();
    
private:
    int64_t clock()
    {
        return system_clock::now().time_since_epoch() / std::chrono::microseconds(1);
    }
    
    void dump(map<int, vector<int>> &connections, int index, const char *indent);
};

#endif /* perf_h */
