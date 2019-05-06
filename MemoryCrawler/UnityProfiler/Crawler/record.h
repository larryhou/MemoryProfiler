//
//  record.h
//  UnityProfiler
//
//  Created by larryhou on 2019/5/5.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef record_h
#define record_h

#include <fstream>
#include <string>
#include <vector>
#include <stack>
#include <map>

#include "stream.h"
#include "crawler.h"
#include "perf.h"

struct StackSample
{
    int32_t id;
    int32_t nameRef;
    int32_t callsCount;
    int32_t gcAllocBytes;
    float totalTime;
    float selfTime;
};

struct RenderFrame
{
    int32_t index;
    float time;
    float fps;
    
    int32_t offset;
};

class RecordCrawler
{
private:
    int64_t __startTime;
    int64_t __stopTime;
    
    FileStream __fs;
    TimeSampler<std::nano> __sampler;
    
    std::vector<std::string> __strings;
    int64_t __strOffset;
    
    int32_t __cursor;
    InstanceManager<RenderFrame> __frames;
    int32_t __lowerFrameIndex;
    int32_t __upperFrameIndex;
    
public:
    RecordCrawler();
    
    void load(const char *filepath);
    
    void inspectFrame(int32_t frameIndex);
    void inspectFrame();
    
    void next();
    void prev();
    
    void summary();
    
    ~RecordCrawler();
    
private:
    void loadStrings();
    void crawl();
    void dumpFrameStacks(int32_t entity, std::vector<StackSample> &samples, std::map<int32_t, std::vector<int32_t>> &relations, const float depthTime, const char *indent = "");
};

#endif /* record_h */
