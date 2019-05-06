//
//  record.cpp
//  UnityProfiler
//
//  Created by larryhou on 2019/5/5.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "record.h"

RecordCrawler::RecordCrawler()
{
    
}

void RecordCrawler::load(const char *filepath)
{
    __sampler.begin("RecordCrawler::load");
    __fs.open(filepath);
    
    auto mime = __fs.readString((size_t)3);
    assert(mime == "PFC");
    
    __startTime = __fs.readUInt64();
    __strOffset = __fs.readUInt32();
    
    auto offset = __fs.tell();
    
    loadStrings();
    
    __fs.seek(offset, seekdir_t::beg);
    crawl();
    
    __sampler.end();
    __sampler.summary();
}

void RecordCrawler::crawl()
{
    __sampler.begin("RecordCrawler::crawl");
    while (__fs.tell() < __strOffset)
    {
        RenderFrame &frame = __frames.add();
        frame.offset = (int32_t)__fs.tell();
        frame.index = __fs.readUInt32();
        frame.time = __fs.readFloat();
        frame.fps = __fs.readFloat();
        
        auto sampleCount = __fs.readUInt32();
        __fs.ignore(sampleCount * 24);
        
        auto relationCount = __fs.readUInt32();
        __fs.ignore(relationCount * 8);
        
        assert(__fs.readUInt32() == 0x12345678);
    }
    
    __lowerFrameIndex = __frames[0].index;
    __upperFrameIndex = __lowerFrameIndex + __frames.size();
    __cursor = __lowerFrameIndex;
    
    __sampler.end();
}

void RecordCrawler::summary()
{
    Statistics<float> fps;
    for (auto i = 0; i < __frames.size(); i++)
    {
        auto &frame = __frames[i];
        fps.collect(frame.fps);
    }
    
    fps.summarize();
    
    auto f = (double)__startTime * 1E-6;
    auto t = (double)__stopTime * 1E-6;
    printf("frames=[%d, %d)=%d elapse=(%.3f, %.3f)=%.3fs fps=%.1f±%.3f=[%.1f, %.1f]\n", __lowerFrameIndex, __upperFrameIndex, __upperFrameIndex - __lowerFrameIndex, f, t, t - f, fps.mean, fps.sd, fps.min, fps.max);
}

void RecordCrawler::findFramesWithFPS(float fps, std::function<bool (float, float)> predicate)
{
    for (auto i = 0; i < __frames.size(); i++)
    {
        auto &frame = __frames[i];
        if (predicate(frame.fps, fps))
        {
            printf("[FRAME] index=%d time=%.3fms fps=%.1f offset=%d\n", frame.index, frame.time, frame.fps, frame.offset);
        }
    }
}

void RecordCrawler::iterateSamples(std::function<void (int32_t, StackSample &)> callback, bool clearProgress)
{
    __fs.seek(__frames[0].offset, seekdir_t::beg);
    auto frameCount = __upperFrameIndex - __lowerFrameIndex;
    
    int32_t iterCount = 0;
    std::cout << " 0%" << std::flush;
    
    double step = 2;
    double progress = 0.0;
    while (__fs.tell() < __strOffset)
    {
        auto index = __fs.readUInt32();
        __fs.readFloat(); // time
        __fs.readFloat(); // fps
        
        readSamples([&](auto &samples, auto &relations)
                    {
                        for (auto i = 0; i < samples.size(); i++)
                        {
                            callback(index, samples[i]);
                        }
                    });
        
        ++iterCount;
        auto percent = (double)iterCount * 100.0 / (double)frameCount;
        if (percent - progress >= step || percent + 1E-4 >= 100)
        {
            printf("\b\b\b\b█%3.0f%%", percent);
            std::cout << std::flush;
            progress += step;
        }
        
        if (iterCount >= frameCount) {break;}
    }
    
    clearProgress? printf("\r%60s\r", " ") : printf("\n");
}

void RecordCrawler::generateStatistics(int32_t rank)
{
    std::map<int32_t, int32_t> callsStat;
    std::map<int32_t, float> timeStat;
    std::vector<int32_t> functions;
    
    double totalTime = 0;
    iterateSamples([&](int32_t index, StackSample &sample)
                   {
                       auto iter = callsStat.find(sample.nameRef);
                       if (iter == callsStat.end())
                       {
                           iter = callsStat.insert(std::pair<int32_t, int32_t>(sample.nameRef, 0)).first;
                           timeStat.insert(std::pair<int32_t, float>(sample.nameRef, 0));
                           functions.push_back(sample.nameRef);
                       }
                       
                       iter->second += sample.callsCount;
                       timeStat.at(sample.nameRef) += sample.selfTime;
                       totalTime += sample.selfTime;
                   });
    
    std::sort(functions.begin(), functions.end(), [&](auto a, auto b)
              {
                  return timeStat.at(a) > timeStat.at(b);
              });
    
    char progress[300+1];
    char fence[] = "█";
    
    auto width = 0;
    for (auto i = 0; i < functions.size(); i++)
    {
        if (rank > 0 && i >= rank){break;}
        auto index = functions[i];
        auto &name = __strings[index];
        width = std::max((int32_t)name.size(), width);
    }
    
    char header[7];
    memset(header, 0, sizeof(header));
    sprintf(header, "%%%ds", width);
    
    for (auto i = 0; i < functions.size(); i++)
    {
        if (rank > 0 && i >= rank){break;}
        auto index = functions[i];
        auto &name = __strings[index];
        printf(header, name.c_str());
        
        memset(progress, 0, sizeof(progress));
        auto time = timeStat.at(index);
        auto percent = time * 100 / totalTime;
        auto count = std::max(1, (int32_t)std::round(percent));
        char *iter = progress;
        for (auto n = 0; n < count; n++)
        {
            memcpy(iter, fence, 3);
            iter += 3;
        }
        printf(" %s %.3f%% %.3fms #%d\n", progress, percent, time, callsStat.at(index));
    }
}

void RecordCrawler::findFramesWithAlloc(int32_t frameOffset, int32_t frameCount)
{
    if (frameOffset >= __lowerFrameIndex && frameOffset < __upperFrameIndex)
    {
        __fs.seek(__frames[frameOffset - __lowerFrameIndex].offset, seekdir_t::beg);
        frameCount = frameCount > 0 ? std::min(__upperFrameIndex - frameOffset, frameCount) : (__upperFrameIndex - frameOffset);
    }
    else
    {
        __fs.seek(__frames[0].offset, seekdir_t::beg);
        frameCount = __upperFrameIndex - __lowerFrameIndex;
    }
    if (frameCount == 0) {return;}
    
    int32_t iterCount = 0;
    
    using std::get;
    std::vector<std::tuple<int32_t, float, float, int32_t, int32_t>> frames;
    
    std::cout << " 0%" << std::flush;
    
    double step = 2;
    double progress = 0.0;
    while (__fs.tell() < __strOffset)
    {
        auto offset = __fs.tell();
        auto index = __fs.readUInt32();
        auto time = __fs.readFloat();
        auto fps = __fs.readFloat();
        
        auto alloc = 0;
        readSamples([&](auto &samples, auto &relations)
                  {
                      for (auto i = 0; i < samples.size(); i++)
                      {
                          StackSample &s = samples[i];
                          alloc += s.gcAllocBytes;
                      }
                  });
        if (alloc > 0)
        {
            frames.emplace_back(std::make_tuple(index, time, fps, alloc, offset));
        }
        
        ++iterCount;
        auto percent = (double)iterCount * 100.0 / (double)frameCount;
        if (percent - progress >= step || percent + 1E-4 >= 100)
        {
            printf("\b\b\b\b█%3.0f%%", percent);
            std::cout << std::flush;
            progress += step;
        }
        
        if (iterCount >= frameCount) {break;}
    }
    printf("\n");
    
    for (auto iter = frames.begin(); iter != frames.end(); iter++)
    {
        auto &f = *iter;
        printf("[FRAME] index=%d time=%.3fms fps=%.1f alloc=%d offset=%d\n", get<0>(f), get<1>(f), get<2>(f), get<3>(f), get<4>(f));
    }
}

void RecordCrawler::loadStrings()
{
    __sampler.begin("RecordCrawler::loadStrings");
    __sampler.begin("seek");
    __fs.seek(__strOffset, seekdir_t::beg);
    __sampler.end();
    __sampler.begin("read");
    auto count = __fs.readUInt32();
    for (auto i = 0; i < count; i++)
    {
        auto size = __fs.readUInt32();
        auto item = __fs.readString((size_t)size);
        __strings.emplace_back(item);
    }
    
    __stopTime = __fs.readUInt64();
    __sampler.end();
    __sampler.end();
}

void RecordCrawler::readSamples(std::function<void (std::vector<StackSample> &, std::map<int32_t, std::vector<int32_t> > &)> completion)
{
    auto elementCount = __fs.readUInt32();
    
    std::vector<StackSample> samples(elementCount);
    for (auto i = 0; i < elementCount; i++)
    {
        StackSample &s = samples[i];
        s.id = __fs.readUInt32();
        s.nameRef = __fs.readUInt32();
        s.callsCount = __fs.readUInt32();
        s.gcAllocBytes = __fs.readUInt32();
        s.totalTime = __fs.readFloat();
        s.selfTime = __fs.readFloat();
    }
    
    elementCount = __fs.readUInt32();
    std::map<int32_t, std::vector<int32_t>> relations;
    std::map<int32_t, int32_t> connections;
    for (auto i = 0; i < elementCount; i++)
    {
        auto node = __fs.readUInt32();
        auto parent = __fs.readUInt32();
        auto match = relations.find(parent);
        if (match == relations.end())
        {
            match = relations.insert(std::pair<int32_t, std::vector<int32_t>>(parent, std::vector<int32_t>())).first;
        }
        match->second.push_back(node);
        connections.insert(std::pair<int32_t, int32_t>(node, parent));
    }
    
    assert(__fs.readUInt32() == 0x12345678);
    
    relations.insert(std::pair<int32_t, std::vector<int32_t>>(-1, std::vector<int32_t>()));
    auto &root = relations.at(-1);
    for (auto iter = relations.begin(); iter != relations.end(); iter++)
    {
        if (iter->first >= 0 && connections.find(iter->first) == connections.end())
        {
            root.push_back(iter->first);
        }
    }
    
    completion(samples, relations);
}

void RecordCrawler::inspectFrame(int32_t frameIndex)
{
    if (frameIndex <  __lowerFrameIndex) {return;}
    if (frameIndex >= __upperFrameIndex) {return;}
    __cursor = frameIndex;
    
    auto &frame = __frames[__cursor - __lowerFrameIndex];
    printf("[FRAME] index=%d time=%.3fms fps=%.1f offset=%d\n", frame.index, frame.time, frame.fps, frame.offset);
    
    __fs.seek(frame.offset + 12, seekdir_t::beg);
    readSamples([&](auto &samples, auto &relations)
              {
                  dumpFrameStacks(-1, samples, relations, frame.time);
              });
}

void RecordCrawler::dumpFrameStacks(int32_t entity, std::vector<StackSample> &samples, std::map<int32_t, std::vector<int32_t> > &relations, const float depthTime, const char *indent)
{
    auto __size = strlen(indent);
    char __indent[__size + 2*3 + 1]; // indent + 2×tabulator + \0
    memset(__indent, 0, sizeof(__indent));
    memcpy(__indent, indent, __size);
    char *tabular = __indent + __size;
    memcpy(tabular + 3, "─", 3);
    
    auto match = relations.find(entity);
    if (match != relations.end())
    {
        auto __time = 0.0;
        auto &children = match->second;
        for (auto i = children.begin(); i != children.end(); i++)
        {
            auto closed = i + 1 == children.end();
            
            closed ? memcpy(tabular, "└", 3) : memcpy(tabular, "├", 3);
            auto &s = samples[*i];
            auto &name = __strings[s.nameRef];
            printf("\e[36m%s%s \e[33mtime=%.3f%%/%.3fms \e[32mself=%.3f%%/%.3fms \e[37mcalls=%d\e[0m", __indent, name.c_str(), s.totalTime * 100 / depthTime, s.totalTime, s.selfTime * 100/s.totalTime, s.selfTime, s.callsCount);
            if (s.gcAllocBytes > 0) {printf(" \e[31malloc=%d", s.gcAllocBytes);}
            printf("\n");
            
            __time += s.totalTime;
            
            if (closed)
            {
                char __nest_indent[__size + 1 + 2 + 1]; // indent + space + 2×space + \0
                memcpy(__nest_indent, indent, __size);
                memset(__nest_indent + __size, '\x20', 3);
                memset(__nest_indent + __size + 3, 0, 1);
                dumpFrameStacks(*i, samples, relations, s.totalTime, __nest_indent);
            }
            else
            {
                char __nest_indent[__size + 3 + 2 + 1]; // indent + tabulator + 2×space + \0
                char *iter = __nest_indent + __size;
                memcpy(__nest_indent, indent, __size);
                memcpy(iter, "│", 3);
                memset(iter + 3, '\x20', 2);
                memset(iter + 5, 0, 1);
                dumpFrameStacks(*i, samples, relations, s.totalTime, __nest_indent);
            }
        }
    }
}

void RecordCrawler::inspectFrame()
{
    if (__cursor < __lowerFrameIndex || __cursor >= __upperFrameIndex)
    {
        __cursor = __lowerFrameIndex;
    }
    
    inspectFrame(__cursor);
}

void RecordCrawler::next(int32_t step)
{
    inspectFrame(__cursor + step);
}

void RecordCrawler::prev(int32_t step)
{
    inspectFrame(__cursor - step);
}

RecordCrawler::~RecordCrawler()
{
    
}
