//
//  FileStream.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/3.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "stream.h"

constexpr static char __hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

FileStream::FileStream()
{
    
}

void FileStream::open(const char* filepath)
{
    if (__fs != nullptr) { delete __fs; }
    
    __fs = new ifstream();
    __fs->open(filepath, ifstream::in | ifstream::binary);
}

void FileStream::seek(size_t offset, seekdir_t whence)
{
    __fs->seekg(offset, whence);
}

size_t FileStream::tell() const
{
    return __fs->tellg();
}

float FileStream::readFloat()
{
    __fs->read(__buf, 4);
    return *(float *)__buf;
}

double FileStream::readDouble()
{
    __fs->read(__buf, 8);
    return *(double *)__buf;
}

int8_t FileStream::readInt8()
{
    __fs->read(__buf, 1);
    return *(int8_t *)__buf;
}

int16_t FileStream::readInt16()
{
    __fs->read(__buf, 2);
    return *(int16_t *)__buf;
}

int32_t FileStream::readInt32()
{
    __fs->read(__buf, 4);
    return *(int32_t *)__buf;
}

int64_t FileStream::readInt64()
{
    __fs->read(__buf, 8);
    return *(int64_t *)__buf;
}

uint8_t FileStream::readUInt8()
{
    __fs->read(__buf, 1);
    return *(uint8_t *)__buf;
}

uint16_t FileStream::readUInt16()
{
    __fs->read(__buf, 2);
    return *(uint16_t *)__buf;
}

uint32_t FileStream::readUInt32()
{
    __fs->read(__buf, 4);
    return *(uint32_t *)__buf;
}

uint64_t FileStream::readUInt64()
{
    __fs->read(__buf, 8);
    return *(uint64_t *)__buf;
}

string FileStream::readUUID()
{
    char uuid[16];
    __fs->read(uuid, 16);
    auto offset = 0;
    for (auto i = 0; i < 16; ++i)
    {
        auto index = i << 1;
        auto c = uuid[i];
        __buf[index + offset] = __hexmap[c >> 4 & 0xF];
        __buf[index + 1 + offset] = __hexmap[c & 0x0F];
        if (i >= 3 && (i + 1) % 2 == 0 && offset < 4)
        {
            __buf[index + 2 + offset] = '-';
            offset += 1;
        }
    }
    string s(__buf, 36);
    return s;
}

string FileStream::readString()
{
    auto size = readUInt32();
    return readString(size);
}

string FileStream::readString(size_t size)
{
    __fs->read(__buf, size);
    string s(__buf, size);
    return s;
}

unicode_t FileStream::readUnicodeString()
{
    auto size = readUInt32();
    return readUnicodeString(size);
}

unicode_t FileStream::readUnicodeString(size_t size)
{
    __fs->read(__buf, size << 1);
    unicode_t s((char16_t *)__buf, size);
    return s;
}

int32_t FileStream::swap(int32_t value)
{
    int32_t v;
    auto raw = (char *)&value;
    auto ptr = (char *)&v;
    ptr[0] = raw[3];
    ptr[1] = raw[2];
    ptr[2] = raw[1];
    ptr[3] = raw[0];
    return v;
}

FileStream::~FileStream()
{
    delete __fs;
}
