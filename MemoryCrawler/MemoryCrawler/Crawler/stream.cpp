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

void FileStream::open(const char* filepath, bool memoryCache)
{
    if (__is != nullptr) { delete __is; }
    
    if (memoryCache)
    {
        ifstream fs;
        fs.open(filepath, ifstream::in | ifstream::binary);
        fs.seekg(0, seekdir_t::end);
        
        auto length = fs.tellg();
        char *bytes = new char[length];
        
        fs.seekg(0);
        fs.read(bytes, length);
        fs.close();
        
        __memory = new MemoryBuffer(bytes, bytes+length);
        __is = new std::istream(__memory);
        __bytes = bytes;
    }
    else
    {
        auto fs = new ifstream();
        fs->open(filepath, ifstream::in | ifstream::binary);
        __is = fs;
    }
}

void FileStream::close()
{
    auto ptr = (ifstream *)__is;
    if (ptr != nullptr)
    {
        ptr->close();
    }
}

bool FileStream::byteAvailable()
{
    return !__is->eof();
}

void FileStream::ignore(size_t size)
{
    auto block = sizeof(__buf);
    while (size >= block)
    {
        __is->read(__buf, block);
        size -= block;
    }
    
    if (size > 0)
    {
        __is->read(__buf, size);
    }
}

void FileStream::seek(size_t offset, seekdir_t whence)
{
    __is->seekg(offset, whence);
}

size_t FileStream::tell() const
{
    return __is->tellg();
}

void FileStream::read(char *buffer, size_t size)
{
    __is->read(buffer, size);
}

float FileStream::readFloat()
{
    __is->read(__buf, 4);
    return *(float *)__buf;
}

double FileStream::readDouble()
{
    __is->read(__buf, 8);
    return *(double *)__buf;
}

int8_t FileStream::readInt8()
{
    __is->read(__buf, 1);
    return *(int8_t *)__buf;
}

int16_t FileStream::readInt16()
{
    __is->read(__buf, 2);
    return *(int16_t *)__buf;
}

int32_t FileStream::readInt32()
{
    __is->read(__buf, 4);
    return *(int32_t *)__buf;
}

int64_t FileStream::readInt64()
{
    __is->read(__buf, 8);
    return *(int64_t *)__buf;
}

uint8_t FileStream::readUInt8()
{
    __is->read(__buf, 1);
    return *(uint8_t *)__buf;
}

uint16_t FileStream::readUInt16()
{
    __is->read(__buf, 2);
    return *(uint16_t *)__buf;
}

uint32_t FileStream::readUInt32()
{
    __is->read(__buf, 4);
    return *(uint32_t *)__buf;
}

uint64_t FileStream::readUInt64()
{
    __is->read(__buf, 8);
    return *(uint64_t *)__buf;
}

string FileStream::readUUID()
{
    char uuid[16];
    __is->read(uuid, 16);
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

void FileStream::skipString()
{
    size_t size = readUInt32();
    __is->read(__buf, size);
}

void FileStream::skipString(bool reverseEndian)
{
    size_t size = readUInt32(reverseEndian);
    __is->read(__buf, size);
}

string FileStream::readString(bool reverseEndian)
{
    size_t size = readUInt32(reverseEndian);
    return readString(size);
}

string FileStream::readString()
{
    size_t size = readUInt32();
    return readString(size);
}

string FileStream::readString(size_t size)
{
    __is->read(__buf, size);
    string s(__buf, size);
    return s;
}

void FileStream::skipUnicodeString()
{
    size_t size = readUInt32();
    __is->ignore(size << 1);
}

void FileStream::skipUnicodeString(bool reverseEndian)
{
    size_t size = readUInt32(reverseEndian);
    __is->ignore(size << 1);
}

unicode_t FileStream::readUnicodeString(bool reverseEndian)
{
    size_t size = readUInt32(reverseEndian);
    return readUnicodeString(size);
}

unicode_t FileStream::readUnicodeString()
{
    size_t size = readUInt32();
    return readUnicodeString(size);
}

unicode_t FileStream::readUnicodeString(size_t size)
{
    __is->read(__buf, size << 1);
    unicode_t s((char16_t *)__buf, size);
    return s;
}

bool FileStream::readBoolean()
{
    __is->read(__buf, 1);
    return __buf[0] != 0;
}

FileStream::~FileStream()
{
    delete __memory;
    delete __bytes;
    delete __is;
}
