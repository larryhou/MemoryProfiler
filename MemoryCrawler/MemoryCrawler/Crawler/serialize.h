//
//  serialize.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/4.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef serialize_h
#define serialize_h

#include <stdio.h>
#include "types.h"
#include "stream.h"
#include "snapshot.h"

class MemorySnapshotReader
{
    FileStream *__fs;
    VirtualMachineInformation *vm;
    FieldDescription *cachedPtr;
    
public:
    string *mime;
    string *unityVersion;
    string *description;
    string *systemVersion;
    string *uuid;
    size_t size;
    int64_t createTime;
    PackedMemorySnapshot *snapshot;
    
    void read(const char *filepath, bool memoryCache = false);
    
    ~MemorySnapshotReader();
    
private:
    void readHeader(FileStream &fs);
    void readSnapshot(FileStream &fs);
    void postSnapshot();
};

#endif /* serialize_h */
