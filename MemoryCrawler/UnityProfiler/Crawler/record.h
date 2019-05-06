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

#include "stream.h"
#include "crawler.h"

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
    
    std::vector<std::string> __strings;
    int64_t __strOffset;
    
    int32_t __frameCursor;
    InstanceManager<RenderFrame> __frames;
    int32_t __startFrameIndex;
    int32_t __stopFrameIndex;
    
public:
    RecordCrawler();
    
    void load(const char *filepath);
    
    void inspectFrame(int32_t frameIndex);
    void inspectFrame();
    
    void next();
    void prev();
    
    ~RecordCrawler();
    
private:
    void loadStrings();
    void process();
};

#endif /* record_h */
