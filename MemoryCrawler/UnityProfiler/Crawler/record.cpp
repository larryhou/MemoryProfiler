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
    
    readMetadatas();
    __dataOffset = __fs.tell();
    
    readStrings();
    
    __fs.seek(__dataOffset, std::ios::beg);
    
    crawl();
    
    __sampler.end();
    __sampler.summarize();
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
        
        auto extra = __fs.readUInt16();
        if (extra > 0)
        {
            frame.hasMemoryInfo = true;
            auto pos = __fs.tell();
            frame.usedHeap = __fs.readUInt64();
            frame.usedMonoHeap = __fs.readUInt64();
            frame.reservedMonoHeap = __fs.readUInt64();
            frame.totalAllocatedMemory = __fs.readUInt64();
            frame.totalReservedMemory = __fs.readUInt64();
            frame.totalUnusedReservedMemory = __fs.readUInt64();
            assert(__fs.tell() - pos == extra);
        }
        else { frame.hasMemoryInfo = false; }
        
        for (auto iter = __metadatas.begin(); iter != __metadatas.end(); iter++)
        {
            auto size = iter->second.size();
            assert(iter->first == __fs.readUInt8());
            AreaStatistics statistics;
            statistics.type = (ProfilerArea)iter->first;
            
            for (auto i = 0; i < size; i++)
            {
                statistics.properties.push_back(__fs.readFloat());
            }
            
            frame.statistics.graphs.emplace_back(statistics);
        }
        
        auto sampleCount = __fs.readUInt32();
        __fs.ignore(sampleCount * 24);
        
        auto relationCount = __fs.readUInt32();
        __fs.ignore(relationCount * 8);
        
        assert(__fs.readUInt32() == 0x12345678);
    }
    
    __lowerFrameIndex = __frames[0].index;
    __upperFrameIndex = __lowerFrameIndex + __frames.size();
    __range = std::make_tuple(__lowerFrameIndex, __upperFrameIndex);
    __cursor = __lowerFrameIndex;
    
    __sampler.end();
}

void RecordCrawler::lock(int32_t frameIndex, int32_t frameCount)
{
    auto lower = std::get<0>(__range);
    auto upper = std::get<1>(__range);
    if (frameIndex >= lower && frameIndex < upper)
    {
        frameCount = frameCount > 0 ? std::min(upper - frameIndex, frameCount) : (upper - frameIndex);
    }
    else
    {
        frameCount = upper - lower;
        frameIndex = lower;
    }
    
    if (frameCount == 0)
    {
        __lowerFrameIndex = lower;
        __upperFrameIndex = upper;
    }
    else
    {
        __lowerFrameIndex = frameIndex;
        __upperFrameIndex = frameIndex + frameCount;
    }
    
    printf("frames=[%d, %d)\n", __lowerFrameIndex, __upperFrameIndex);
}

void RecordCrawler::summarize(bool rangeEnabled)
{
    if (rangeEnabled)
    {
        Statistics<float> fps;
        auto baseIndex = std::get<0>(__range);
        for (auto i = __lowerFrameIndex; i < __upperFrameIndex; i++)
        {
            auto &frame = __frames[i - baseIndex];
            fps.collect(frame.fps);
        }
        
        fps.summarize();
        printf("frames=[%d, %d)=%d fps=%.1f±%.1f range=[%.1f, %.1f] reasonable=[%.1f, %.1f]\n", __lowerFrameIndex, __upperFrameIndex, __upperFrameIndex - __lowerFrameIndex, fps.mean, 3 * fps.standardDeviation, fps.minimum, fps.maximum, fps.reasonableMinimum, fps.reasonableMaximum);
    }
    else
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
        auto lower = std::get<0>(__range);
        auto upper = std::get<1>(__range);
        printf("frames=[%d, %d)=%d elapse=(%.3f, %.3f)=%.3fs fps=%.1f±%.1f range=[%.1f, %.1f] reasonable=[%.1f, %.1f]\n", lower, upper, upper - lower, f, t, t - f, fps.mean, 3 * fps.standardDeviation, fps.minimum, fps.maximum, fps.reasonableMinimum, fps.reasonableMaximum);
    }
}

void RecordCrawler::findFramesWithFPS(float fps, std::function<bool (float, float)> predicate)
{
    auto baseIndex = std::get<0>(__range);
    for (auto i = __lowerFrameIndex; i < __upperFrameIndex; i++)
    {
        auto &frame = __frames[i - baseIndex];
        if (predicate(frame.fps, fps))
        {
            printf("[FRAME] index=%d time=%.3fms fps=%.1f offset=%d\n", frame.index, frame.time, frame.fps, frame.offset);
        }
    }
}

void RecordCrawler::iterateSamples(std::function<void (int32_t, StackSample &)> callback, bool clearProgress)
{
    auto &frame = __frames[__lowerFrameIndex - std::get<0>(__range)];
    __fs.seek(frame.offset, std::ios::beg);
    
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
        
        __fs.ignore(__fs.readUInt16() + __statsize);
        
        readFrameSamples([&](auto &samples, auto &relations)
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
    
    std::cout << (clearProgress ? '\r' : '\n');
}

void RecordCrawler::statByFunction(int32_t rank)
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

    if (functions.size() == 0)
    {
        printf("INFO: no function found!\n");
        return;
    }
    
    Statistics<int32_t> width;
    for (auto i = 0; i < functions.size(); i++)
    {
        if (rank > 0 && i >= rank){break;}
        auto index = functions[i];
        auto &name = __strings[index];
        width.collect((int32_t)name.size());
    }
    width.summarize();
    
    char header[8];
    memset(header, 0, sizeof(header));
    sprintf(header, " %%-%ds", width.reasonableMaximum);
    
    for (auto i = 0; i < functions.size(); i++)
    {
        if (rank > 0 && i >= rank){break;}
        auto index = functions[i];
        auto &name = __strings[index];
        
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
        printf("%5.2f%% %9.2fms #%-8d", percent, time, callsStat.at(index));
        std::cout << progress;
        printf(" %s *%d\n", name.c_str(), index);
    }
}

void RecordCrawler::findFramesWithFunction(int32_t functionNameRef)
{
    std::vector<int32_t> results;
    iterateSamples([&](int32_t index, StackSample &sample)
                   {
                       if (sample.nameRef == functionNameRef)
                       {
                           results.push_back(index);
                       }
                   }, false);
    auto baseIndex = std::get<0>(__range);
    for (auto i = results.begin(); i != results.end(); i++)
    {
        auto &frame = __frames[*i - baseIndex];
        printf("[FRAME] index=%d time=%.3fms fps=%.1f offset=%d\n", frame.index, frame.time, frame.fps, frame.offset);
    }
}

void RecordCrawler::inspectFunction(int32_t functionNameRef)
{
    std::vector<StackSample> samples;
    std::vector<int32_t> frames;
    
    Statistics<float> stats;
    iterateSamples([&](int32_t index, StackSample &sample)
                   {
                       if (sample.nameRef == functionNameRef)
                       {
                           samples.emplace_back(sample);
                           frames.push_back(index);
                           
                           stats.collect(sample.totalTime);
                       }
                   }, true);
    stats.summarize();
    
    auto &name = __strings[functionNameRef];
    printf("[%s] count=%d total=%.3f mean=%.3f±%.3f \n", name.c_str(), stats.size(), stats.sum, stats.mean, stats.standardDeviation);
    
    auto baseIndex = std::get<0>(__range);
    stats.iterateUnusualMaximums([&](int32_t index, float value)
                                 {
                                     auto frameIndex = frames[index];
                                     auto &frame = __frames[frameIndex - baseIndex];
                                     printf("%7.3f [FRAME] index=%d time=%.3fms fps=%.1f offset=%d\n", value ,frame.index, frame.time, frame.fps, frame.offset);
                                 });
}

void RecordCrawler::findFramesMatchValue(ProfilerArea area, int32_t property, float value, std::function<bool (float, float)> predicate)
{
    auto &check = __frames[0].statistics.graphs;
    if (check.size() <= (int32_t)area) {return;}
    if (check[0].properties.size() <= property) {return;}
    
    auto baseIndex = std::get<0>(__range);
    
    std::vector<int32_t> results;
    for (auto i = __lowerFrameIndex; i < __upperFrameIndex; i++)
    {
        auto index = i - baseIndex;
        auto &frame = __frames[index];
        auto flag = predicate(frame.statistics.graphs[area].properties[property], value);
        if (flag) {results.push_back(i);}
    }
    for (auto i = results.begin(); i != results.end(); i++)
    {
        auto &frame = __frames[*i - baseIndex];
        printf("[FRAME] index=%d time=%.3fms fps=%.1f offset=%d\n", frame.index, frame.time, frame.fps, frame.offset);
    }
}

void RecordCrawler::statValues(ProfilerArea area, int32_t property)
{
    auto &check = __frames[0].statistics.graphs;
    if (check.size() <= (int32_t)area) {return;}
    if (check[0].properties.size() <= property) {return;}
    
    auto baseIndex = std::get<0>(__range);
    
    Statistics<float> stats;
    for (auto i = __lowerFrameIndex; i < __upperFrameIndex; i++)
    {
        auto index = i - baseIndex;
        auto &frame = __frames[index];
        stats.collect(frame.statistics.graphs[area].properties[property]);
    }
    
    stats.summarize();
    
    printf("[%s][%s]", __names[area].c_str(), __metadatas.at(area)[property].c_str());
    printf(" mean=%.3f±%.3f range=[%.0f, %.0f] reasonable=[%.0f, %.0f]\n", stats.mean, stats.standardDeviation, stats.minimum, stats.maximum, stats.reasonableMinimum, stats.reasonableMaximum);
}

void RecordCrawler::findFramesWithAlloc(int32_t frameOffset, int32_t frameCount)
{
    if (frameOffset < 0) {frameOffset = 0;}
    if (frameCount <= 0)
    {
        frameCount = __upperFrameIndex - __lowerFrameIndex;
    }
    else
    {
        frameCount = std::min(frameCount, __upperFrameIndex - __lowerFrameIndex);
    }
    if (frameCount == 0) {return;}
    
    auto baseIndex = std::get<0>(__range);
    auto frameIndex = __lowerFrameIndex + frameOffset;
    __fs.seek(__frames[frameIndex - baseIndex].offset, std::ios::beg);
    
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
        
        __fs.ignore(__fs.readUInt16() + __statsize);
        
        auto alloc = 0;
        readFrameSamples([&](auto &samples, auto &relations)
                  {
                      for (auto i = 0; i < samples.size(); i++)
                      {
                          StackSample &s = samples[i];
                          if (s.selfTime == s.totalTime)
                          {
                              alloc += s.gcAllocBytes;
                          }
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
    printf("\r");
    
    for (auto iter = frames.begin(); iter != frames.end(); iter++)
    {
        auto &f = *iter;
        printf("[FRAME] index=%d time=%.3fms fps=%.1f alloc=%d offset=%d\n", get<0>(f), get<1>(f), get<2>(f), get<3>(f), get<4>(f));
    }
}

void RecordCrawler::readStrings()
{
    __sampler.begin("RecordCrawler::loadStrings");
    __sampler.begin("seek");
    __fs.seek(__strOffset, std::ios::beg);
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

void RecordCrawler::readMetadatas()
{
    __statsize = 0;
    auto size = __fs.readUInt32();
    
    auto offset = __fs.tell();
    auto count = __fs.readUInt8();
    for (auto i = 0; i < count; i++)
    {
        auto area = (ProfilerArea)__fs.readUInt8();
        auto name = __fs.readString();
        __names.insert(std::pair<ProfilerArea, string>(area, name));
        auto iter = __metadatas.insert(std::pair<ProfilerArea, std::vector<string>>(area, std::vector<string>())).first;
        auto itemCount = __fs.readUInt8();
        
        __statsize += 1 + itemCount * 4;
        for (auto i = 0; i < itemCount; i++)
        {
            string name = __fs.readString();
            iter->second.emplace_back(name);
        }
    }
    
    assert(__fs.tell() - offset == size);
}

void RecordCrawler::readFrameSamples(std::function<void (std::vector<StackSample> &, std::map<int32_t, std::vector<int32_t> > &)> completion)
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
    
    if (relations.find(-1) == relations.end())
    {
       relations.insert(std::pair<int32_t, std::vector<int32_t>>(-1, std::vector<int32_t>()));
    }
    
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

void RecordCrawler::inspectFrame(int32_t frameIndex, int32_t depth)
{
    if (frameIndex <  std::get<0>(__range)) {return;}
    if (frameIndex >= std::get<1>(__range)) {return;}
    __cursor = frameIndex;
    __cdepth = depth;
    
    auto &frame = __frames[__cursor - std::get<0>(__range)];
    
    __fs.seek(frame.offset + 12, std::ios::beg);
    __fs.ignore(__fs.readUInt16() + __statsize);
    readFrameSamples([&](auto &samples, auto &relations)
                     {
                         int32_t alloc = 0;
                         for (auto i = samples.begin(); i != samples.end(); i++)
                         {
                             StackSample &s = *i;
                             if (s.gcAllocBytes > 0 && s.totalTime == s.selfTime)
                             {
                                 alloc += s.gcAllocBytes;
                             }
                         }
                         printf("[FRAME] index=%d time=%.3fms fps=%.1f alloc=%d", frame.index, frame.time, frame.fps, alloc);
                         if (frame.hasMemoryInfo)
                         {
                             printf(" usedHeap=%llu monoHeap=%llu usedMono=%llu totalAllocated=%llu totalReserved=%llu totalUnused=%llu",
                                    frame.usedHeap, frame.reservedMonoHeap, frame.usedMonoHeap, frame.totalAllocatedMemory, frame.totalReservedMemory, frame.totalUnusedReservedMemory);
                         }
                         printf("\n");
                         dumpFrameStacks(-1, samples, relations, frame.time, depth);
                     });
    auto &statistics = frame.statistics.graphs;
    for (auto i = 0; i < statistics.size(); i++)
    {
        auto area = (ProfilerArea)i;
        auto &item = statistics[i];
        auto &propeties = __metadatas.at(area);
        std::cout << (i % 2 == 0 ? "\e[32m" : "\e[33m");
        printf("[%18s]", __names.at(area).c_str());
        for (auto n = 0; n < item.properties.size(); n++)
        {
            auto &p = item.properties[n];
            printf(" '%s'=%.0f", propeties[n].c_str(), p);
        }
        printf("\n");
    }
}

void RecordCrawler::dumpFrameStacks(int32_t entity, std::vector<StackSample> &samples, std::map<int32_t, std::vector<int32_t> > &relations, const float totalTime, const int32_t depth, const char *indent, const int32_t __depth)
{
    auto __size = strlen(indent);
    NEW_CHAR_ARR(__indent, __size + 2*3 + 1); // indent + 2×tabulator + \0
    memset(GET_CHAR_ARR_PTR(__indent), 0, sizeof(__indent));
    memcpy(GET_CHAR_ARR_PTR(__indent), indent, __size);
    char *tabular = GET_CHAR_ARR_PTR(__indent) + __size;
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
            printf("\e[36m%s%s \e[33mtime=%.3f%%/%.3fms \e[32mself=%.3f%%/%.3fms \e[37mcalls=%d", __indent, name.c_str(), s.totalTime * 100 / totalTime, s.totalTime, s.selfTime * 100/s.totalTime, s.selfTime, s.callsCount);
            if (s.gcAllocBytes > 0) {printf(" \e[31malloc=%d", s.gcAllocBytes);}
            printf(" \e[90m*%d\e[0m\n", s.nameRef);
            
            __time += s.totalTime;
            
            if (closed)
            {
                char* __nest_indent = new char[__size + 1 + 2 + 1]; // indent + space + 2×space + \0
                memcpy(__nest_indent, indent, __size);
                memset(__nest_indent + __size, '\x20', 3);
                memset(__nest_indent + __size + 3, 0, 1);
                if (depth <= 0 || __depth + 1 < depth)
                {
                    dumpFrameStacks(*i, samples, relations, s.totalTime, depth, __nest_indent, __depth + 1);
                }
                delete[] __nest_indent;
            }
            else
            {
                char* __nest_indent = new char[__size + 3 + 2 + 1]; // indent + tabulator + 2×space + \0
                char *iter = __nest_indent + __size;
                memcpy(__nest_indent, indent, __size);
                memcpy(iter, "│", 3);
                memset(iter + 3, '\x20', 2);
                memset(iter + 5, 0, 1);
                if (depth <= 0 || __depth + 1 < depth)
                {
                    dumpFrameStacks(*i, samples, relations, s.totalTime, depth, __nest_indent, __depth + 1);
                }
                delete[] __nest_indent;
            }
        }
    }
}

void RecordCrawler::dumpMetadatas()
{
    for (auto iter = __metadatas.begin(); iter != __metadatas.end(); iter++)
    {
        auto &name = __names[iter->first];
        auto &children = iter->second;
        printf("[%02d]'%s'\n", iter->first, name.c_str());
        
        for (auto i = 0; i < children.size(); i++)
        {
            printf("    [%d]'%s'\n", i, children[i].c_str());
        }
    }
}

void RecordCrawler::inspectFrame(int32_t frameIndex)
{
    if (frameIndex < 0) {frameIndex = __cursor;}
    if (frameIndex < __lowerFrameIndex)
    {
        frameIndex = __lowerFrameIndex;
    }
    else if (frameIndex >= __upperFrameIndex)
    {
        frameIndex = __upperFrameIndex - 1;
    }
    
    inspectFrame(frameIndex, 0);
}

void RecordCrawler::next(int32_t step, int32_t depth)
{
    inspectFrame(__cursor + step, depth < 0 ? __cdepth : depth);
}

void RecordCrawler::prev(int32_t step, int32_t depth)
{
    inspectFrame(__cursor - step, depth < 0 ? __cdepth : depth);
}

void RecordCrawler::list(int32_t frameOffset, int32_t frameCount, int32_t sorting)
{
    if (frameOffset < 0) {frameOffset = 0;}
    if (frameCount <= 0)
    {
        frameCount = __upperFrameIndex - __lowerFrameIndex;
    }
    else
    {
        frameCount = std::min(frameCount, __upperFrameIndex - __lowerFrameIndex);
    }
    if (frameCount == 0) {return;}
    
    std::vector<int32_t> slice;
    
    auto baseIndex = std::get<0>(__range);
    auto frameIndex = __lowerFrameIndex + frameOffset;
    
    Statistics<float> stats;
    for (auto i = 0; i < frameCount; i++)
    {
        auto index = (frameIndex + i) - baseIndex;
        auto &frame = __frames[index];
        stats.collect(frame.fps);
        slice.push_back(index);
    }
    stats.summarize();
    
    if (sorting < 0)
    {
        std::sort(slice.begin(), slice.end(), [&](int32_t a, int32_t b)
                  {
                      return __frames[a].fps < __frames[b].fps;
                  });
    }
    else if (sorting > 0)
    {
        std::sort(slice.begin(), slice.end(), [&](int32_t a, int32_t b)
                  {
                      return __frames[a].fps > __frames[b].fps;
                  });
    }
    
    for (auto i = slice.begin(); i != slice.end(); i++)
    {
        auto index = *i;
        auto &frame = __frames[index];
        printf("[FRAME] index=%d time=%.3fms fps=%.1f offset=%d\n", frame.index, frame.time, frame.fps, frame.offset);
    }
    
    printf("\e[37m[SUMMARY] fps=%.1f±%.1f range=[%.1f, %.1f] reasonable=[%.1f, %.1f]", stats.mean, 3 * stats.standardDeviation, stats.minimum, stats.maximum, stats.reasonableMinimum, stats.reasonableMaximum);
    std::vector<RenderFrame *> excepts;
    for (auto i = 0; i < frameCount; i++)
    {
        auto index = (frameIndex + i) - baseIndex;
        auto &frame = __frames[index];
        if (frame.fps < stats.minimum) { excepts.push_back(&frame); }
    }
    if (excepts.size() > 0)
    {
        printf(" excepts=");
        for (auto i = excepts.begin(); i != excepts.end(); i++)
        {
            auto &frame = **i;
            printf(" %d|%.1f", frame.index, frame.fps);
        }
    }
    printf("\n");
    
}

RecordCrawler::~RecordCrawler()
{
    
}
