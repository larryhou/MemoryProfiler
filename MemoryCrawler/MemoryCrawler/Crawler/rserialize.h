//
//  rserialize.h
//  MemoryCrawler
//
//  Created by LARRYHOU on 2019/10/23.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef rserialize_h
#define rserialize_h

#include <stdio.h>
#include <cassert>
#include "serialize.h"
#include "types.h"

class RawMemorySnapshotReader: public MemorySnapshotDeserializer
{
    PackedMemorySnapshot *__snapshot;
public:
    uint32_t formatVersion;
    
public:
    RawMemorySnapshotReader(const char* filepath);
    ~RawMemorySnapshotReader();
    
    void read(PackedMemorySnapshot &snapshot) override;
};

#endif /* rserialize_h */
