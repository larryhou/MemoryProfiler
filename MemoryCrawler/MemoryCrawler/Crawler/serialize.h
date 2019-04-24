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
#include "perf.h"

class MemorySnapshotReader
{
    FileStream *__fs = nullptr;
    VirtualMachineInformation *__vm = nullptr;
    FieldDescription *__cachedPtr = nullptr;
    const char *__filepath = nullptr;
    PackedMemorySnapshot *__snapshot = nullptr;
    TimeSampler<std::nano> __sampler;
    
public:
    string mime;
    string unityVersion;
    string description;
    string systemVersion;
    string uuid;
    size_t size;
    int64_t createTime;
    
    MemorySnapshotReader(const char *filepath);
    PackedMemorySnapshot &read(PackedMemorySnapshot &snapshot);
    
    ~MemorySnapshotReader();
    
private:
    void readHeader(FileStream &fs);
    void readSnapshot(FileStream &fs);
    void postSnapshot();
    void summarize();
    
    void readPackedMemorySnapshot(PackedMemorySnapshot &item, FileStream &fs);
};

#endif /* serialize_h */
