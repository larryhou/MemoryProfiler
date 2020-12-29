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
#include <tuple>
#include <map>

#include "stream.h"
#include "crawler.h"
#include "perf.h"
#include "stat.h"

enum ProfilerArea:int32_t {
    PA_CPU = 0,
    PA_GPU,
    PA_Rendering,
    PA_Memory,
    PA_Audio,
    PA_Video,
    PA_Physics,
    PA_Physics2D,
    PA_NetworkMessages,
    PA_NetworkOperations,
    PA_UI,
    PA_UIDetails,
    PA_GlobalIllumination,
    PA_AreaCount
};

struct AreaStatistics
{
    ProfilerArea type;
    std::vector<float> properties;
};

struct FrameStatistics
{
    std::vector<AreaStatistics> graphs;
};

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
    
    uint64_t usedHeap;
    uint64_t reservedMonoHeap;
    uint64_t usedMonoHeap;
    
    uint64_t totalAllocatedMemory;
    uint64_t totalReservedMemory;
    uint64_t totalUnusedReservedMemory;
    bool hasMemoryInfo;
    
    FrameStatistics statistics;
    
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
    size_t __strOffset;
    
    std::map<ProfilerArea, string> __names;
    std::map<ProfilerArea, std::vector<string>> __metadatas;
    size_t __dataOffset;
    int32_t __statsize;
    
    int32_t __cursor;
    int32_t __cdepth;
    
    InstanceManager<RenderFrame> __frames;
    int32_t __lowerFrameIndex;
    int32_t __upperFrameIndex;
    std::tuple<int32_t, int32_t> __range;
    
public:
    RecordCrawler();
    
    void load(const char *filepath);
    
    void inspectFrame(int32_t frameIndex, int32_t depth);
    void inspectFrame(int32_t frameIndex);
    
    void list(int32_t frameOffset = -1, int32_t frameCount = 10, int32_t sorting = 0);
    
    void iterateSamples(std::function<void(int32_t, StackSample &)> callback, bool clearProgress = true);
    void statByFunction(int32_t rank = 0);
    
    void next(int32_t step = 1, int32_t depth = -1);
    void prev(int32_t step = 1, int32_t depth = -1);
    
    void lock(int32_t frameIndex = -1, int32_t frameCount = -1);
    
    void findFramesWithFPS(float fps, std::function<bool(float a, float b)> predicate);
    void findFramesWithAlloc(int32_t frameOffset = -1, int32_t frameCount = -1);
    void findFramesWithFunction(int32_t functionNameRef);
    
    void inspectFunction(int32_t functionNameRef);
    
    void findFramesMatchValue(ProfilerArea area, int32_t property, float value, std::function<bool(float a, float b)> predicate);
    void statValues(ProfilerArea area, int32_t property);
    
    void dumpMetadatas();
    
    void summarize(bool rangeEnabled);
    
    ~RecordCrawler();
    
private:
    void readStrings();
    void readMetadatas();
    void crawl();
    void readFrameSamples(std::function<void(std::vector<StackSample> &, std::map<int32_t, std::vector<int32_t>> &)> completion);
    void dumpFrameStacks(int32_t entity, std::vector<StackSample> &samples, std::map<int32_t, std::vector<int32_t>> &relations, const float totalTime, const int32_t depth = 0, const char *indent = "",  const int32_t __depth = 0);
};

#endif /* record_h */
