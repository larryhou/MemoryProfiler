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

//typedef duration<long long,         nano> nanoseconds;
//typedef duration<long long,        micro> microseconds;
//typedef duration<long long,        milli> milliseconds;
//typedef duration<long long  ratio<   1> > seconds;
//typedef duration<     long, ratio<  60> > minutes;
//typedef duration<     long, ratio<3600> > hours;

// std::nano
// std::micro
// std::milli
// std::ratio<1>
// std::ratio<60>
// std::ratio<3600>

template <class T>
class TimeSampler
{
    vector<const char *> __events;
    
    map<int, clocktime_t> __timestamps;
    map<int, clocktime_t> __records;
    
    vector<int> __entities;
    map<int, int> __bridges;
    vector<int> __cursors;
    
public:
    TimeSampler();
    
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
        return std::chrono::duration_cast<std::chrono::duration<long long, T>>(time - base).count();
    }
    
    void dump(map<int, vector<int>> &connections, int index, const char *indent);
};

template <class T>
TimeSampler<T>::TimeSampler()
{
    
}

template <class T>
int TimeSampler<T>::begin(const char *event)
{
    auto timestamp = clock();
    
    auto sequence = (int)__events.size();
    
    __events.push_back(event);
    __timestamps.insert(pair<int, clocktime_t>(sequence, timestamp));
    
    auto cursorCount = __cursors.size();
    if (cursorCount == 0)
    {
        __entities.push_back(sequence);
    }
    else
    {
        __bridges.insert(pair<int, int>(sequence, __cursors[cursorCount - 1]));
    }
    
    __cursors.push_back(sequence);
    return sequence;
}

template <class T>
int64_t TimeSampler<T>::end()
{
    auto timestamp = clock();
    
    auto cursorCount = __cursors.size();
    if (cursorCount == 0) {return 0;}
    
    auto sequence = __cursors[cursorCount - 1];
    __records.insert(pair<int, clocktime_t>(sequence, timestamp));
    __cursors.pop_back();
    
    return duration(timestamp, __timestamps[sequence]);
}

template <class T>
void TimeSampler<T>::summary()
{
    assert(__records.size() == __timestamps.size());
    if (__records.size() == 0) {return;}
    
    map<int, vector<int>> connections;
    for (auto it = __bridges.begin(); it != __bridges.end(); it++)
    {
        auto leaf = it->first;
        auto branch = it->second;
        
        auto entry = connections.find(branch);
        if (entry == connections.end())
        {
            vector<int> vec{leaf};
            connections.insert(pair<int, vector<int>>(branch, vec));
        }
        else
        {
            entry->second.push_back(leaf);
        }
    }
    
    for(auto i = 0; i < __entities.size(); i++)
    {
        dump(connections, __entities[i], "");
    }
}

template <class T>
void TimeSampler<T>::dump(map<int, vector<int>> &connections, int index, const char *indent)
{
    printf("%s[%d] %s=%lld\n", indent, index, __events[index], duration(__records.at(index), __timestamps.at(index)));
    
    auto iter = connections.find(index);
    if (iter != connections.end())
    {
        auto children = iter->second;
        for (auto i = 0; i < children.size(); i++)
        {
            char nestIndent[strlen(indent) + 4];
            strcpy(nestIndent, indent);
            
            dump(connections, children[i], strcat(nestIndent, "    "));
        }
    }
}

#endif /* perf_h */
