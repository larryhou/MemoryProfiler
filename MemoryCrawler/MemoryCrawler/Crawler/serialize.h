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

class MemorySnapshotDeserializer
{
protected:
    const char *__filepath;
    FileStream __fs;
    TimeSampler<std::nano> __sampler;
    PackedMemorySnapshot *__snapshot;
    VirtualMachineInformation *__vm;
    FieldDescription *__cachedPtr;
public:
    string uuid;
    
public:
    MemorySnapshotDeserializer(const char *filepath)
    {
        auto buffer = new char[strlen(filepath)];
        std::strcpy(buffer, filepath);
        __filepath = buffer;
    }
    
    virtual ~MemorySnapshotDeserializer()
    {
        delete [] __filepath;
        __fs.close();
    }
    
    virtual void read(PackedMemorySnapshot &snapshot);
protected:
    void prepareSnapshot();
    void finishSnapshot();
};

class MemorySnapshotReader: public MemorySnapshotDeserializer
{
public:
    string mime;
    string unityVersion;
    string description;
    string systemVersion;
    size_t size;
    int64_t createTime;
    
    MemorySnapshotReader(const char *filepath);
    ~MemorySnapshotReader();
    
    void read(PackedMemorySnapshot &snapshot) override;
    
private:
    void readHeader(FileStream &fs);
    void readSnapshot(FileStream &fs);
    void readPackedMemorySnapshot(PackedMemorySnapshot &item, FileStream &fs);
};

#endif /* serialize_h */
