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
    __sampler.end();
}

void RecordCrawler::summary()
{
    auto f = (double)__startTime * 1E-6;
    auto t = (double)__stopTime * 1E-6;
    printf("frames=[%d, %d)=%d elapse=(%.3f, %.3f)=%.3f\n", __lowerFrameIndex, __upperFrameIndex, __upperFrameIndex - __lowerFrameIndex, f, t, t - f);
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

void RecordCrawler::inspectFrame(int32_t frameIndex)
{
    if (frameIndex <  __lowerFrameIndex) {return;}
    if (frameIndex >= __upperFrameIndex) {return;}
    __cursor = frameIndex;
    
    auto &frame = __frames[__cursor - __lowerFrameIndex];
    
    __fs.seek(frame.offset + 12, seekdir_t::beg);
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
    printf("[FRAME] index=%d time=%.3fms fps=%.1f offset=%d\n", frame.index, frame.time, frame.fps, frame.offset);
    
    relations.insert(std::pair<int32_t, std::vector<int32_t>>(-1, std::vector<int32_t>()));
    auto &root = relations.at(-1);
    for (auto iter = relations.begin(); iter != relations.end(); iter++)
    {
        if (iter->first >= 0 && connections.find(iter->first) == connections.end())
        {
            root.push_back(iter->first);
        }
    }
    
    dumpFrameStacks(-1, samples, relations, frame.time);
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
            if (s.gcAllocBytes > 0) {printf(" \e[31m%d", s.gcAllocBytes);}
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

void RecordCrawler::next()
{
    if (__cursor + 1 < __upperFrameIndex)
    {
        inspectFrame(__cursor + 1);
    }
}

void RecordCrawler::prev()
{
    if (__cursor - 1 >= __lowerFrameIndex)
    {
        inspectFrame(__cursor - 1);
    }
}

RecordCrawler::~RecordCrawler()
{
    
}
