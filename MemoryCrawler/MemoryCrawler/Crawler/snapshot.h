//
//  snapshot.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/2.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef snapshot_h
#define snapshot_h

#include <string>
#include "types.h"

using std::string;

struct Connection
{
    int32_t from;
    int32_t to;
    
    ~Connection();
};

struct FieldDescription
{
    bool isStatic;
    int16_t slotIndex;
    int32_t offset;
    int32_t typeIndex;
    int32_t hostTypeIndex = -1;
    string *name;
    
    ~FieldDescription();
};

struct TypeDescription
{
    address_t typeInfoAddress;
    string *assembly;
    FieldDescription* fields; // FieldDescription[]
    string *name;
    const byte_t *staticFieldBytes; // byte[]
    int32_t arrayRank;
    int32_t baseOrElementTypeIndex;
    int32_t size;
    int32_t typeIndex;
    
    int32_t instanceCount = 0;
    int32_t managedMemory = 0;
    int32_t nativeMemory = 0;
    int32_t nativeTypeArrayIndex = -1;
    
    bool isArray;
    bool isValueType;
    bool isUnityEngineObjectType;
    
    ~TypeDescription();
};

struct MemorySection
{
    byte_t *bytes; // byte[]
    address_t startAddress;
    int32_t heapArrayIndex = -1;
    
    ~MemorySection();
};

struct PackedGCHandle
{
    address_t target;
    
    int32_t gcHandleArrayIndex = -1;
    int32_t managedObjectArrayIndex = -1;
    
    ~PackedGCHandle();
};

struct PackedNativeUnityEngineObject
{
    int32_t hideFlags;
    int32_t instanceId;
    bool isDontDestroyOnLoad;
    bool isManager;
    bool isPersistent;
    string *name;
    address_t nativeObjectAddress;
    int32_t nativeTypeArrayIndex;
    int32_t size;
    int32_t classId;
    
    int32_t managedObjectArrayIndex = -1;
    int32_t nativeObjectArrayIndex = -1;
    
    ~PackedNativeUnityEngineObject();
};

struct PackedNativeType
{
    string *name;
    int32_t nativeBaseTypeArrayIndex;
    int32_t baseClassId;
    
    int32_t typeIndex = -1;
    int32_t managedTypeArrayIndex = -1;
    int32_t instanceCount = 0;
    int32_t nativeMemory = 0;
    
    ~PackedNativeType();
};

struct VirtualMachineInformation
{
    int32_t allocationGranularity;
    int32_t arrayBoundsOffsetInHeader;
    int32_t arrayHeaderSize;
    int32_t arraySizeOffsetInHeader;
    int32_t heapFormatVersion;
    int32_t objectHeaderSize;
    int32_t pointerSize;
    
    ~VirtualMachineInformation();
};

struct PackedMemorySnapshot
{
    Connection *connections; // Connection[]
    PackedGCHandle *gcHandles; // PackedGCHandle[]
    MemorySection *managedHeapSections; // MemorySection[]
    PackedNativeUnityEngineObject *nativeObjects; // PackedNativeUnityEngineObject[]
    PackedNativeType *nativeTypes; // PackedNativeType[]
    TypeDescription *typeDescriptions; // TypeDescription[]
    VirtualMachineInformation *virtualMachineInformation;
    
    FieldDescription *cached_ptr;
    string *uuid;
    
    ~PackedMemorySnapshot();
};

#endif /* snapshot_h */
