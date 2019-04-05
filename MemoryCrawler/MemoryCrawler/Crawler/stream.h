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
#include "types.h"

using std::string;
using std::u16string;
using std::u32string;

using std::ifstream;

class FileStream
{
public:
    void open(const char* filepath);
    void close();
    
    bool byteAvailable();
    
    size_t tell() const;
    void seek(size_t offset, seekdir_t whence);
    
    void read(char *buffer, size_t size);
    
    float readFloat();
    double readDouble();
    
    string readUUID();
    string readString();
    string readString(bool reverseEndian);
    string readString(size_t size);
    
    unicode_t readUnicodeString();
    unicode_t readUnicodeString(bool reverseEndian);
    unicode_t readUnicodeString(size_t size);
    
    uint64_t readUInt64(bool reverseEndian) { return readValue<uint64_t>(8, reverseEndian); }
    uint32_t readUInt32(bool reverseEndian) { return readValue<uint32_t>(4, reverseEndian); }
    uint16_t readUInt16(bool reverseEndian) { return readValue<uint16_t>(2, reverseEndian); }
    
    int64_t readInt64(bool reverseEndian) { return readValue<int64_t>(8, reverseEndian); }
    int32_t readInt32(bool reverseEndian) { return readValue<int32_t>(4, reverseEndian); }
    int16_t readInt16(bool reverseEndian) { return readValue<int16_t>(2, reverseEndian); }
    
    uint64_t readUInt64();
    uint32_t readUInt32();
    uint16_t readUInt16();
    uint8_t readUInt8();
    
    int64_t readInt64();
    int32_t readInt32();
    int16_t readInt16();
    int8_t readInt8();
    
    bool readBoolean();
    
    FileStream();
    
    ~FileStream();
private:
    ifstream* __fs;
    char __buf[64*1024];
    
    template <typename T>
    T readValue(int size, bool reverseEndian);
};

template <typename T>
T FileStream::readValue(int size, bool reverseEndian)
{
    __fs->read(__buf, size);
    if (!reverseEndian)
    {
        return *(T *)__buf;
    }
    else
    {
        for (auto i = 0; i < size; i++)
        {
            __buf[size + i] = __buf[size - i - 1];
        }
        
        return *(T *)(__buf + size);
    }
}

#endif /* stream_h */
