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
using std::ifstream;

class FileStream
{
public:
    void open();
    
    size_t tell() const;
    void seek(size_t offset, seekdir_t whence);
    
    byte_t* read(size_t size);
    
    float readFloat();
    double readDouble();
    
    string* readString();
    string* readString(size_t size);
    
    string* readUnicodeString();
    string* readUnicodeString(size_t size);
    
    uint64_t readUInt64();
    uint32_t readUInt32();
    uint16_t readUInt16();
    uint8_t readUInt8();
    
    int64_t readInt64();
    int32_t readInt32();
    int16_t readInt16();
    int8_t readInt8();
    
    bool readBoolean();
    
    ~FileStream();
private:
    size_t position;
    ifstream* input;
};

#endif /* stream_h */
