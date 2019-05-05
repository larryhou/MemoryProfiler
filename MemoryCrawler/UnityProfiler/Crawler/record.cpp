//
//  profiler.cpp
//  UnityProfiler
//
//  Created by larryhou on 2019/5/5.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "record.hpp"

RecordCrawler::RecordCrawler()
{
    
}

void RecordCrawler::load(const char *filepath)
{
    __fs.open(filepath);
    
    auto mime = __fs.readString((size_t)3);
    assert(mime == "PFC");
    
    __startTime = __fs.readUInt64();
    __strOffset = __fs.readUInt32();
    
    auto offset = __fs.tell();
    
    loadStrings();
    
    __fs.seek(offset, seekdir_t::beg);
    process();
}

void RecordCrawler::process()
{
    while (__fs.tell() < __strOffset)
    {
        UnityTickFrame &frame = __frames.add();
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
    
    __startFrameIndex = __frames[0].index;
    __stopFrameIndex = __startFrameIndex + __frames.size();
}

void RecordCrawler::loadStrings()
{
    __fs.seek(__strOffset, seekdir_t::beg);
    auto count = __fs.readUInt32();
    for (auto i = 0; i < count; i++)
    {
        auto size = __fs.readUInt32();
        auto item = __fs.readString((size_t)size);
        __strings.emplace_back(item);
    }
    
    __stopTime = __fs.readUInt64();
}

void RecordCrawler::inspectFrame(int32_t frameIndex)
{
    __frameCursor = frameIndex;
}

void RecordCrawler::inspectFrame()
{
    if (__frameCursor < __startFrameIndex || __frameCursor >= __stopFrameIndex)
    {
        __frameCursor = __startFrameIndex;
    }
    
    inspectFrame(__frameCursor);
}

void RecordCrawler::next()
{
    if (__frameCursor + 1 < __stopFrameIndex)
    {
        inspectFrame(__frameCursor + 1);
    }
}

void RecordCrawler::prev()
{
    if (__frameCursor - 1 >= __startFrameIndex)
    {
        inspectFrame(__frameCursor - 1);
    }
}

RecordCrawler::~RecordCrawler()
{
    
}
