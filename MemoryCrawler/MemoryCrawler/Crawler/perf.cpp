//
//  perf.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/4.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "perf.h"

int TimeSampler::begin(const char *event)
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

int64_t TimeSampler::end()
{
    auto timestamp = clock();
    
    auto cursorCount = __cursors.size();
    if (cursorCount == 0) {return 0;}
    
    auto sequence = __cursors[cursorCount - 1];
    __records.insert(pair<int, clocktime_t>(sequence, timestamp));
    __cursors.pop_back();
    
    return duration(timestamp, __timestamps[sequence]);
}

void TimeSampler::summary()
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

void TimeSampler::dump(map<int, vector<int>> &connections, int index, const char *indent)
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


