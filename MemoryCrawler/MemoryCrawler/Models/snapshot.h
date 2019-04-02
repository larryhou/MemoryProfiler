//
//  snapshot.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/2.
//  Copyright © 2019 larryhou. All rights reserved.
//
#include <string>
using std::string;

#ifndef snapshot_h
#define snapshot_h

struct FieldDescription
{
    bool isStatic;
    int16_t slotIndex;
    int32_t offset;
    int32_t typeIndex;
    int32_t hostTypeIndex;
    string* name;
};

struct TypeDescription
{
    int64_t typeInfoAddress;
    string* assembly;
    FieldDescription* fields; // FieldDescription[]
    string* name;
    const char* staticFieldBytes; // Byte[]
    int32_t arrayRank;
    int32_t baseOrElementTypeIndex;
    int32_t size;
    int32_t typeIndex;
    int32_t instanceCount;
    int32_t managedMemory;
    int32_t nativeMemory;
    int32_t nativeTypeArrayIndex;
    
    bool isArray;
    bool isValueType;
    bool isUnityEngineObjectType;
};

#endif /* snapshot_h */
