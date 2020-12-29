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

void FileStream::open(const char* filepath, std::ios_base::openmode mode)
{
    __fs.open(filepath, mode);
}

void FileStream::open(const char* filepath)
{
    open(filepath, std::fstream::in | std::fstream::binary);
}

void FileStream::close()
{
    __fs.close();
}

bool FileStream::byteAvailable()
{
    return !__fs.eof();
}

void FileStream::ignore(size_t size)
{
    auto block = sizeof(__buf);
    while (size >= block)
    {
        __fs.read(__buf, block);
        size -= block;
    }
    
    if (size > 0)
    {
        __fs.read(__buf, size);
    }
}

void FileStream::seek(size_t offset, seekdir_t whence)
{
    __fs.seekg(offset, whence);
}

size_t FileStream::tell()
{
    return __fs.tellg();
}

void FileStream::read(char *buffer, size_t size)
{
    __fs.read(buffer, size);
}

string FileStream::readUUID()
{
    char uuid[16];
    __fs.read(uuid, 16);
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
    __fs.read(__buf, size);
}

void FileStream::skipString(bool reverseEndian)
{
    size_t size = readUInt32(reverseEndian);
    __fs.read(__buf, size);
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
    __fs.read(__buf, size);
    string s(__buf, size);
    return s;
}

string FileStream::readZEString()
{
    *__buf = 0;
    auto ptr = __buf;
    do
    {
        __fs.read(ptr, 1);
    } while (*ptr++ != 0);
    return __buf;
}

void FileStream::skipUnicodeString()
{
    size_t size = readUInt32();
    __fs.read(__buf, size << 1);
}

void FileStream::skipUnicodeString(bool reverseEndian)
{
    size_t size = readUInt32(reverseEndian);
    __fs.read(__buf, size << 1);
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
    __fs.read(__buf, size << 1);
    unicode_t s((char16_t *)__buf, size);
    return s;
}

bool FileStream::readBoolean()
{
    __fs.read(__buf, 1);
    return __buf[0] != 0;
}

void FileStream::write(const char *v)
{
    __fs.write(v, strlen(v));
}

void FileStream::write(const char *v, int32_t size)
{
    if (size > 0) { __fs.write(v, size); }
}

void FileStream::writeUTFString(const char *v)
{
    auto size = (int32_t)strlen(v);
    write<int32_t>(size);
    write(v, size);
}

void FileStream::writeUTFString(const char* v, bool reverseEndian)
{
    auto size = (int32_t)strlen(v);
    write<int32_t>(size, reverseEndian);
    write(v, size);
}

// BinaryWriter::Write(System.String)
void FileStream::writeBWString(const char* v)
{
    auto size = (int32_t)strlen(v);
    auto byte = size;
    while (byte >= 0x80)
    {
        write<char>((byte & 0x7F) | 0x80);
        byte >>= 7;
    }
    write<char>(byte);
    write(v, size);
}

FileStream::~FileStream()
{
    delete __memory;
    delete __bytes;
}
