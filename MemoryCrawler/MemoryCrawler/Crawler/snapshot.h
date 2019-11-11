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

enum ConnectionKind:uint8_t { CK_none = 0, CK_gcHandle, CK_static, CK_managed, CK_native, CK_link };
enum MemoryState:uint8_t { MS_none = 0, MS_persistent, MS_allocated };

struct Connection
{
    int32_t connectionArrayIndex;
    int32_t from;
    int32_t to;
    ConnectionKind fromKind = CK_none;
    ConnectionKind toKind = CK_none;
    
    ~Connection();
};

struct NativeManagedLink
{
    int32_t nativeTypeArrayIndex;
    int32_t nativeArrayIndex;
    address_t nativeAddress;
    address_t managedAddress;
    address_t managedTypeAddress;
};

struct NativeVector2
{
    float x, y;
};

struct NativeVector3: public NativeVector2
{
    float z;
};

struct NativeRect
{
    float x, y, width, height;
};

struct NativeQuaternion
{
    float x, y, z, w;
};

struct NativeObject
{
    int32_t nativeArrayIndex;
};

struct NativeTransform: public NativeObject
{
    NativeVector3 position;
    NativeVector3 localPosition;
    NativeQuaternion rotation;
    NativeQuaternion localRotation;
    NativeVector3 scale;
    address_t parent;
};

struct NativeRectTransform: public NativeTransform
{
    NativeRect rect;
    NativeVector2 anchorMin;
    NativeVector2 anchorMax;
    NativeVector2 sizeDelta;
    NativeVector2 anchoredPosition;
    NativeVector2 pivot;
};

struct NativeComponent: public NativeObject
{
    address_t address;
    bool behaviour;
    bool enabled;
    bool isActiveAndEnabled;
};

struct NativeGameObject: public NativeObject
{
    bool isActive;
    bool isSelfActive;
    std::vector<NativeComponent> components;
};

struct NativeTexture2D: public NativeObject
{
    bool pot;
    uint8_t format;
    uint32_t width, height;
};

struct NativeSprite: public NativeObject
{
    float x, y, width, height;
    NativeVector2 pivot;
    NativeTexture2D *texture;
    int32_t textureNativeArrayIndex;
};

struct NativeAppending
{
    NativeManagedLink link;
    
    int32_t sprite = -1;
    int32_t texture = -1;
    int32_t transform = -1;
    int32_t rectTransform = -1;
    int32_t gameObject = -1;
};

struct NativeAppendingCollection
{
    std::vector<NativeSprite> sprites;
    std::vector<NativeTexture2D> textures;
    std::vector<NativeTransform> transforms;
    std::vector<NativeRectTransform> rectTransforms;
    std::vector<NativeGameObject> gameObjects;
    std::vector<NativeAppending> appendings;
};

struct FieldDescription
{
    bool isStatic;
    int16_t fieldSlotIndex;
    int32_t offset;
    int32_t typeIndex;
    int32_t hookTypeIndex = -1;
    string name;
    
    ~FieldDescription();
};

struct TypeDescription
{
    address_t typeInfoAddress;
    string assembly;
    Array<FieldDescription>* fields = nullptr; // FieldDescription[]
    string name;
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
    std::vector<int32_t> fromConnections;
    std::vector<int32_t> toConnections;
    
    int32_t flags;
    int32_t hideFlags;
    int32_t instanceId;
    bool isDontDestroyOnLoad;
    bool isManager;
    bool isPersistent;
    MemoryState state = MS_none;
    string name;
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
    string name;
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
    Array<MemorySection> *heapSections = nullptr; // MemorySection[]
    Array<MemorySection> *stacksSections = nullptr; // MemorySection[]
    Array<PackedNativeUnityEngineObject> *nativeObjects = nullptr; // PackedNativeUnityEngineObject[]
    Array<PackedNativeType> *nativeTypes = nullptr; // PackedNativeType[]
    Array<TypeDescription> *typeDescriptions = nullptr; // TypeDescription[]
    VirtualMachineInformation virtualMachineInformation;
    NativeAppendingCollection nativeAppendingCollection;
    
    std::vector<MemorySection *> *sortedHeapSections = nullptr;
    
    ManagedTypeIndex managedTypeIndex;
    NativeTypeIndex nativeTypeIndex;
    
    FieldDescription *cached_ptr = nullptr;
    string uuid;
    
    ~PackedMemorySnapshot();
};

#endif /* snapshot_h */
