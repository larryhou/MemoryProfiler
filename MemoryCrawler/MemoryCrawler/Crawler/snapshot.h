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
#include <vector>
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
    int16_t fieldSlotIndex;
    int32_t offset;
    int32_t typeIndex;
    int32_t hookTypeIndex = -1;
    string *name;
    
    ~FieldDescription();
};

struct TypeDescription
{
    address_t typeInfoAddress;
    string *assembly = nullptr;
    Array<FieldDescription>* fields = nullptr; // FieldDescription[]
    string *name = nullptr;
    Array<byte_t> *staticFieldBytes = nullptr; // byte[]
    int32_t arrayRank;
    int32_t baseOrElementTypeIndex;
    int32_t size;
    int32_t typeIndex;
    
    int32_t instanceCount = 0;
    int32_t instanceMemory = 0;
    int32_t nativeMemory = 0;
    int32_t nativeTypeArrayIndex = -1;
    
    bool isArray;
    bool isValueType;
    bool isUnityEngineObjectType;
    
    ~TypeDescription();
};

struct MemorySection
{
    Array<byte_t> *bytes = nullptr; // byte[]
    address_t startAddress;
    int32_t heapArrayIndex = -1;
    int32_t size;
    
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
    std::vector<Connection *> fromConnections;
    std::vector<Connection *> toConnections;
    
    int32_t hideFlags;
    int32_t instanceId;
    bool isDontDestroyOnLoad;
    bool isManager;
    bool isPersistent;
    string *name = nullptr;
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
    string *name = nullptr;
    int32_t nativeBaseTypeArrayIndex;
    int32_t baseClassId;
    
    int32_t typeIndex = -1;
    int32_t managedTypeArrayIndex = -1;
    int32_t instanceCount = 0;
    int32_t instanceMemory = 0;
    
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
    Array<Connection> *connections = nullptr; // Connection[]
    Array<PackedGCHandle> *gcHandles = nullptr; // PackedGCHandle[]
    Array<MemorySection> *managedHeapSections = nullptr; // MemorySection[]
    Array<PackedNativeUnityEngineObject> *nativeObjects = nullptr; // PackedNativeUnityEngineObject[]
    Array<PackedNativeType> *nativeTypes = nullptr; // PackedNativeType[]
    Array<TypeDescription> *typeDescriptions = nullptr; // TypeDescription[]
    VirtualMachineInformation virtualMachineInformation;
    
    std::vector<MemorySection *> *sortedHeapSections = nullptr;
    
    ManagedTypeIndex managedTypeIndex;
    NativeTypeIndex nativeTypeIndex;
    
    FieldDescription *cached_ptr = nullptr;
    string uuid;
    
    ~PackedMemorySnapshot();
};

#endif /* snapshot_h */
