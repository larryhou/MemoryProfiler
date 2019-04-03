//
//  stream.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/3.
//  Copyright © 2019 larryhou. All rights reserved.
//
#include <fstream>
#include <string>

using std::ios_base;
using std::string;

using seekdir = std::ios_base::seekdir;

#ifndef stream_h
#define stream_h

struct FileStream
{
    byte_t* read(size_t size);
    
    size_t tell() const;
    void seek(size_t offset, seekdir whence);
    
    float read_float();
    double read_double();
    
    string* read_string();
    string* read_string(size_t size);
    
    string* read_unicode_string();
    string* read_unicode_string(size_t size);
    
    uint64_t read_uint64();
    uint32_t read_uint32();
    uint16_t read_uint16();
    uint8_t read_uint8();
    
    int64_t read_int64();
    int32_t read_int32();
    int16_t read_int16();
    int8_t read_int8();
    
    bool read_boolean();
};

#endif /* stream_h */
