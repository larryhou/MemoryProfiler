//
//  snapshot.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/2.
//  Copyright © 2019 larryhou. All rights reserved.
//
#include <string>
using std::string;

using byte_t = unsigned char;
using address_t = int64_t;

#ifndef snapshot_h
#define snapshot_h

struct Connection
{
    int32_t from;
    int32_t to;
};

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
    address_t typeInfoAddress;
    string* assembly;
    FieldDescription* fields; // FieldDescription[]
    string* name;
    const byte_t* staticFieldBytes; // byte[]
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

struct MemorySection
{
    byte_t* bytes;
    address_t startAddress;
    int32_t heapArrayIndex;
};

struct PackedGCHandle
{
    int32_t gcHandleArrayIndex;
    
    int32_t managedObjectArrayIndex;
};

struct PackedNativeUnityEngineObject
{
    int32_t hideFlags;
    int32_t instanceId;
    bool isDontDestroyOnLoad;
    bool isManager;
    bool isPersistent;
    string* name;
    address_t nativeObjectAddress;
    int32_t nativeTypeArrayIndex;
    int32_t size;
    
    int32_t managedObjectArrayIndex;
    int32_t nativeObjectArrayIndex;
};

struct PackedNativeType
{
    string* name;
    int32_t nativeBaseTypeArrayIndex;
    
    int32_t typeIndex;
    int32_t managedTypeArrayIndex;
    int32_t instanceCount;
    int32_t nativeMemory;
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
};

struct PackedMemorySnapshot
{
    Connection* connections; // Connection[]
    PackedGCHandle* gcHandles; // PackedGCHandle[]
    MemorySection* managedHeapSections; // MemorySection[]
    PackedNativeUnityEngineObject* nativeObjects; // PackedNativeUnityEngineObject[]
    PackedNativeType* nativeTypes; // PackedNativeType[]
    TypeDescription* typeDescriptions; // TypeDescription[]
    VirtualMachineInformation* virtualMachineInformation;
    
    FieldDescription* cached_ptr;
    string* uuid;
};

#endif /* snapshot_h */
