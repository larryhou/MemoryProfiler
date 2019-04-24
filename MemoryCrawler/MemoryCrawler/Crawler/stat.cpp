//
//  stat.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/19.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "stat.h"
using std::pair;

void TrackStatistics::collect(int32_t itemIndex, int32_t typeIndex, int32_t size)
{
    __samples.push_back(std::make_tuple(itemIndex, typeIndex, size));
    auto iter = __memory.find(typeIndex);
    if (iter == __memory.end())
    {
        iter = __memory.insert(pair<int32_t, int32_t>(typeIndex, 0)).first;
    }
    
    iter->second += size;
}

void TrackStatistics::summarize(bool reverse)
{
    std::sort(__samples.begin(), __samples.end(), [&](sample_t &a, sample_t &b)->bool
              {
                  auto ta = std::get<1>(a);
                  auto tb = std::get<1>(b);
                  if (ta != tb)
                  {
                      auto ma = __memory.at(ta);
                      auto mb = __memory.at(tb);
                      if (ma != mb) {return ma < mb;}
                      return ta < tb;
                  }
                  
                  auto sa = std::get<2>(a);
                  auto sb = std::get<2>(b);
                  if (sa != sb) {return sa > sb;}
                  return std::get<0>(a) < std::get<0>(b);
              });
    if (reverse) {this->reverse();}
}

void TrackStatistics::reverse()
{
    std::reverse(__samples.begin(), __samples.end());
}

void TrackStatistics::foreach(std::function<void (int32_t, int32_t, int64_t)> callback, int32_t depth)
{
    int32_t __remain = 0;
    int32_t __depth = 0;
    int32_t __index = -1;
    
    int64_t __typeCount = 0;
    int64_t __skipCount = 0;
    
    for (auto iter = __samples.begin(); iter != __samples.end(); iter++)
    {
        auto sample = *iter;
        auto itemIndex = std::get<0>(sample);
        auto typeIndex = std::get<1>(sample);
        auto size = std::get<2>(sample);
        if (__index != typeIndex)
        {
            if (__index >= 0) { callback(-2, __index, (__skipCount << 16 | __typeCount) << 32 | __remain); } // summary
            callback(-1, typeIndex, __memory.at(typeIndex)); // header
            
            __remain = 0;
            __index = typeIndex;
            __depth = 0;
            
            __typeCount = 0;
            __skipCount = 0;
        }
        
        __typeCount++;
        if (__depth < depth || depth <= 0)
        {
            __depth++;
            callback(itemIndex, typeIndex, size);
        }
        else
        {
            __remain += size;
            __skipCount++;
        }
    }
    
    callback(-2, __index, (__skipCount << 16 | __typeCount) << 32 | __remain);
}
