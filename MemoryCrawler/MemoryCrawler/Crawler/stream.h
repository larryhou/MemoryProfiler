//
//  FileStream.hpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/3.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef stream_h
#define stream_h

#include <fstream>
#include <string>
#include <streambuf>
#include "types.h"

using std::string;
using std::u16string;
using std::u32string;

using std::ifstream;

struct MemoryBuffer: std::streambuf
{
    MemoryBuffer(char *begin, char *end)
    {
        this->setg(begin, begin, end);
    }
};

class FileStream
{
public:
    FileStream();
    ~FileStream();
    
    void open(const char* filepath);
    void open(const char* filepath, std::ios_base::openmode mode);
    void close();
    
    bool byteAvailable();
    
    size_t tell();
    void seek(size_t offset, seekdir_t whence);
    
    void read(char *buffer, size_t size);
    
    void ignore(size_t size);
    
    float readFloat() { return read<float>(); }
    float readFloat(bool reverseEndian) { return read<float>(reverseEndian); }
    
    double readDouble() { return read<double>(); }
    double readDouble(bool reverseEndian) { return read<double>(reverseEndian); }
    
    string readUUID();
    
    void skipString();
    void skipString(bool reverseEndian);
    
    string readString();
    string readString(bool reverseEndian);
    string readString(size_t size);
    
    string readZEString();
    
    void skipUnicodeString();
    void skipUnicodeString(bool reverseEndian);
    
    unicode_t readUnicodeString();
    unicode_t readUnicodeString(bool reverseEndian);
    unicode_t readUnicodeString(size_t size);
    
    uint64_t readUInt64(bool reverseEndian) { return read<uint64_t>(reverseEndian); }
    uint32_t readUInt32(bool reverseEndian) { return read<uint32_t>(reverseEndian); }
    uint16_t readUInt16(bool reverseEndian) { return read<uint16_t>(reverseEndian); }
    
    int64_t readInt64(bool reverseEndian) { return read<int64_t>(reverseEndian); }
    int32_t readInt32(bool reverseEndian) { return read<int32_t>(reverseEndian); }
    int16_t readInt16(bool reverseEndian) { return read<int16_t>(reverseEndian); }
    
    uint64_t readUInt64() { return read<uint64_t>(); }
    uint32_t readUInt32() { return read<uint32_t>(); }
    uint16_t readUInt16() { return read<uint16_t>(); }
    uint8_t readUInt8() { return read<uint8_t>(); }
    
    int64_t readInt64() { return read<int64_t>(); }
    int32_t readInt32() { return read<int32_t>(); }
    int16_t readInt16() { return read<int16_t>(); }
    int8_t readInt8() { return read<int8_t>(); }
    
    template <typename T>
    T read(bool reverseEndian);
    
    template <typename T>
    T read();
    
    bool readBoolean();
    
    // write
    template <typename T>
    void write(T v, bool reverseEndian);
    
    template <typename T>
    void write(T v);
    
    void write(const char* v);
    void write(const char* v, int32_t size);
    
    void writeUTFString(const char* v);
    void writeUTFString(const char* v, bool reverseEndian);
    
    void writeBWString(const char* v);
    
private:
    std::fstream __fs;
    char *__bytes = nullptr;
    char __buf[64*1024];
    MemoryBuffer *__memory = nullptr;
};

template <typename T>
void FileStream::write(T v)
{
    __fs.write((const char*)&v, sizeof(T));
}

template <typename T>
void FileStream::write(T v, bool reverseEndian)
{
    auto size = sizeof(T);
    if (reverseEndian)
    {
        auto ptr = (char*)&v + size - 1;
        for (auto i = 0; i < size; i++) { __fs.write(ptr--, 1); }
    }
    else
    {
        __fs.write((const char*)&v, size);
    }
}

template <typename T>
T FileStream::read()
{
    __fs.read(__buf, sizeof(T));
    return *(T *)__buf;
}

template <typename T>
T FileStream::read(bool reverseEndian)
{
    auto size = sizeof(T);
    if (reverseEndian)
    {
        auto ptr = __buf + size - 1;
        for (auto i = 0; i < size; i++) { __fs.read(ptr--, 1); }
    }
    else
    {
        __fs.read(__buf, size);
    }
    
    return *(T *)__buf;
}

#endif /* stream_h */
