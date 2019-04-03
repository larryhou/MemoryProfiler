//
//  FileStream.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/3.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "stream.h"

FileStream::FileStream()
{
    
}

void FileStream::open(const char* filepath)
{
    ifstream fs;
    fs.open(filepath, ifstream::in);
    stream = &fs;
}

void FileStream::seek(size_t offset, seekdir_t whence)
{
    stream->seekg(offset, whence);
}

size_t FileStream::tell() const
{
    return stream->tellg();
}

float FileStream::readFloat()
{
    stream->read(buffer, 4);
    return *(float *)buffer;
}

double FileStream::readDouble()
{
    stream->read(buffer, 8);
    return *(double *)buffer;
}

int8_t FileStream::readInt8()
{
    stream->read(buffer, 1);
    return *(int8_t *)buffer;
}

int16_t FileStream::readInt16()
{
    stream->read(buffer, 2);
    return *(int16_t *)buffer;
}

int32_t FileStream::readInt32()
{
    stream->read(buffer, 4);
    return *(int32_t *)buffer;
}

int64_t FileStream::readInt64()
{
    stream->read(buffer, 8);
    return *(int64_t *)buffer;
}

uint8_t FileStream::readUInt8()
{
    stream->read(buffer, 1);
    return *(uint8_t *)buffer;
}

uint16_t FileStream::readUInt16()
{
    stream->read(buffer, 2);
    return *(uint16_t *)buffer;
}

uint32_t FileStream::readUInt32()
{
    stream->read(buffer, 4);
    return *(uint32_t *)buffer;
}

uint64_t FileStream::readUInt64()
{
    stream->read(buffer, 8);
    return *(uint64_t *)buffer;
}

string FileStream::readString()
{
    auto size = readUInt32();
    stream->read(buffer, size);
    string s(buffer, size);
    return s;
}

string FileStream::readString(size_t size)
{
    stream->read(buffer, size);
    string s(buffer, size);
    return s;
}

string FileStream::readUnicodeString()
{
    return "";
}

string FileStream::readUnicodeString(size_t size)
{
    return "";
}


FileStream::~FileStream()
{
    
}
