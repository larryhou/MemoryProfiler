//
//  serialize.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/4.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "serialize.h"
#include "perf.h"
#include <map>

static TimeSampler<std::nano> sampler;

void MemorySnapshotReader::read(const char *filepath, bool memoryCache)
{
    __fs = new FileStream;
    __fs->open(filepath, memoryCache);
    readHeader(*__fs);
    while (__fs->byteAvailable())
    {
        auto length = __fs->readUInt32(true);
        auto type = __fs->readUInt8();
        switch (type)
        {
            case '0':
                readSnapshot(*__fs);
            
            default:
                __fs->ignore(length - 5);
                break;
        }
        __fs->ignore(8);
    }
}

void MemorySnapshotReader::readHeader(FileStream &fs)
{
    mime = new string(fs.readString((size_t)3));
    description = new string(fs.readString(true));
    unityVersion = new string(fs.readString(true));
    systemVersion = new string(fs.readString(true));
    uuid = new string(fs.readUUID());
    size = fs.readUInt32(true);
    createTime = fs.readUInt64(true);
}

void readField(FileStream &fs)
{
//    auto fieldName = fs.readString(true);
//    auto fieldType = fs.readString(true);
//    printf("%s %s\n", fieldType.c_str(), fieldName.c_str());
    fs.skipString(true);
    fs.skipString(true);
}

//MARK: read object
static const string sPackedNativeUnityEngineObject("PackedNativeUnityEngineObject");
void readPackedNativeUnityEngineObject(PackedNativeUnityEngineObject &item, FileStream &fs)
{
    fs.skipString(true);
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 10);
    
    readField(fs);
    item.isPersistent = fs.readBoolean();
    readField(fs);
    item.isDontDestroyOnLoad = fs.readBoolean();
    readField(fs);
    item.isManager = fs.readBoolean();
    readField(fs);
    item.name = new string(fs.readString(true));
    readField(fs);
    item.instanceId = fs.readInt32(true);
    readField(fs);
    item.size = fs.readInt32(true);
    readField(fs);
    item.classId = fs.readInt32(true);
    readField(fs);
    item.nativeTypeArrayIndex = fs.readInt32(true);
    readField(fs);
    item.hideFlags = fs.readUInt32(true);
    readField(fs);
    item.nativeObjectAddress = fs.readInt64(true);
}

static const string sPackedNativeType("PackedNativeType");
void readPackedNativeType(PackedNativeType &item, FileStream &fs)
{
    fs.skipString(true);
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 3);
    
    readField(fs);
    item.name = new string(fs.readString(true));
    readField(fs);
    item.baseClassId = fs.readInt32(true);
    readField(fs);
    item.nativeBaseTypeArrayIndex = fs.readInt32(true);
}

static const string sPackedGCHandle("PackedGCHandle");
void readPackedGCHandle(PackedGCHandle &item, FileStream &fs)
{
    fs.skipString(true);
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 1);
    
    readField(fs);
    item.target = fs.readUInt64(true);
}

static const string sConnection("Connection");
void readConnection(Connection &item, FileStream &fs)
{
    fs.skipString(true);
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 2);
    
    readField(fs);
    item.from = fs.readInt32(true);
    readField(fs);
    item.to = fs.readInt32(true);
}

static const string sMemorySection("MemorySection");
void readMemorySection(MemorySection &item, FileStream &fs)
{
    fs.skipString(true);
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 2);
    
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        if (size > 0)
        {
            item.bytes = new Array<byte_t>(size);
            fs.read((char *)(item.bytes->items), size);
        }
    }
    readField(fs);
    item.startAddress = fs.readUInt64(true);
}

static const string sFieldDescription("FieldDescription");
void readFieldDescription(FieldDescription &item, FileStream &fs)
{
    fs.skipString(true);
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 4);
    
    readField(fs);
    item.name = new string(fs.readString(true));
    readField(fs);
    item.offset = fs.readInt32(true);
    readField(fs);
    item.typeIndex = fs.readInt32(true);
    readField(fs);
    item.isStatic = fs.readBoolean();
}

static const string sTypeDescription("TypeDescription");
void readTypeDescription(TypeDescription &item, FileStream &fs)
{
    fs.skipString(true);
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 11);
    
    readField(fs);
    item.isValueType = fs.readBoolean();
    readField(fs);
    item.isArray = fs.readBoolean();
    readField(fs);
    item.arrayRank = fs.readInt32(true);
    readField(fs);
    item.name = new string(fs.readString(true));
    readField(fs);
    item.assembly = new string(fs.readString(true));
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        item.fields = new Array<FieldDescription>(size);
        for (auto i = 0; i < size; i++)
        {
            readFieldDescription(item.fields->items[i], fs);
        }
    }
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        if (size > 0)
        {
            item.staticFieldBytes = new Array<byte_t>(size);
            fs.read((char *)(item.staticFieldBytes->items), size);
        }
    }
    readField(fs);
    item.baseOrElementTypeIndex = fs.readInt32(true);
    readField(fs);
    item.size = fs.readInt32(true);
    readField(fs);
    item.typeInfoAddress = fs.readUInt64(true);
    readField(fs);
    item.typeIndex = fs.readInt32(true);
}

static const string sVirtualMachineInformation("VirtualMachineInformation");
void readVirtualMachineInformation(VirtualMachineInformation &item, FileStream &fs)
{
    fs.skipString(true);
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 7);
    
    readField(fs);
    item.pointerSize = fs.readInt32(true);
    readField(fs);
    item.objectHeaderSize = fs.readInt32(true);
    readField(fs);
    item.arrayHeaderSize = fs.readInt32(true);
    readField(fs);
    item.arrayBoundsOffsetInHeader = fs.readInt32(true);
    readField(fs);
    item.arraySizeOffsetInHeader = fs.readInt32(true);
    readField(fs);
    item.allocationGranularity = fs.readInt32(true);
    readField(fs);
    item.heapFormatVersion = fs.readInt32(true);
}

static const string sPackedMemorySnapshot("PackedMemorySnapshot");
void readPackedMemorySnapshot(PackedMemorySnapshot &item, FileStream &fs)
{
    sampler.begin("readPackedMemorySnapshot");
    sampler.begin("read_type_string");
    fs.skipString(true);
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 7);
    sampler.end();
    
    readField(fs);
    {
        sampler.begin("read_native_types");
        auto size = fs.readUInt32(true);
        item.nativeTypes = new Array<PackedNativeType>(size);
        for (auto i = 0; i < size; i++)
        {
            readPackedNativeType(item.nativeTypes->items[i], fs);
        }
        sampler.end();
    }
    readField(fs);
    {
        sampler.begin("read_native_objects");
        auto size = fs.readUInt32(true);
        item.nativeObjects = new Array<PackedNativeUnityEngineObject>(size);
        for (auto i = 0; i < size; i++)
        {
            readPackedNativeUnityEngineObject(item.nativeObjects->items[i], fs);
        }
        sampler.end();
    }
    readField(fs);
    {
        sampler.begin("read_gc_handles");
        auto size = fs.readUInt32(true);
        item.gcHandles = new Array<PackedGCHandle>(size);
        for (auto i = 0; i < size; i++)
        {
            readPackedGCHandle(item.gcHandles->items[i], fs);
        }
        sampler.end();
    }
    readField(fs);
    {
        sampler.begin("read_connections");
        auto size = fs.readUInt32(true);
        item.connections = new Array<Connection>(size);
        for (auto i = 0; i < size; i++)
        {
            readConnection(item.connections->items[i], fs);
        }
        sampler.end();
    }
    readField(fs);
    {
        sampler.begin("read_heap_sections");
        auto size = fs.readUInt32(true);
        item.managedHeapSections = new Array<MemorySection>(size);
        for (auto i = 0; i < size; i++)
        {
            readMemorySection(item.managedHeapSections->items[i], fs);
        }
        sampler.end();
    }
    readField(fs);
    {
        sampler.begin("read_type_descriptions");
        auto size = fs.readUInt32(true);
        item.typeDescriptions = new Array<TypeDescription>(size);
        for (auto i = 0; i < size; i++)
        {
            readTypeDescription(item.typeDescriptions->items[i], fs);
        }
        sampler.end();
    }
    sampler.begin("read_virtual_matchine_information");
    sampler.begin("read_field_strings");
    readField(fs);
    sampler.end();
    sampler.begin("read_object");
    item.virtualMachineInformation = new VirtualMachineInformation;
    readVirtualMachineInformation(*item.virtualMachineInformation, fs);
    sampler.end();
    sampler.end();
    sampler.end();
}

void MemorySnapshotReader::readSnapshot(FileStream &fs)
{
    vm = new VirtualMachineInformation;
    readVirtualMachineInformation(*vm, fs);
    
    snapshot = new PackedMemorySnapshot;
    readPackedMemorySnapshot(*snapshot, fs);
    
    for (auto i = 0; i < snapshot->managedHeapSections->size; i++)
    {
        MemorySection &heap = snapshot->managedHeapSections->items[i];
        heap.size = heap.bytes->size;
    }
    
    postSnapshot();
}

bool endsWith(const string *s, const string *with)
{
    auto its = s->rbegin();
    auto itw = with->rbegin();
    
    while (itw != with->rend())
    {
        if (*itw != *its) {return false;}
        ++its;
        ++itw;
    }
    
    return true;
}

inline bool readTypeIndex(int32_t &index, const TypeDescription &type, const string *pattern)
{
    if (index == -1 && endsWith(type.name, pattern))
    {
        index = type.typeIndex;
        return true;
    }
    
    return false;
}

void MemorySnapshotReader::postSnapshot()
{
    sampler.begin("create_type_strings");
    string sUnityEngineObject("UnityEngine.Object");
    string sSystemString("System.String");
    string sSystemTextGenerator("UnityEngine.TextGenerator");
    string sCachedPtr("m_CachedPtr");
    sampler.end();
    
    ManagedTypeIndex &managedTypeIndex = snapshot->managedTypeIndex;
    
    sampler.begin("read_type_index");
    bool isUnityEngineObject = false;
    Array<TypeDescription> &typeDescriptions = *snapshot->typeDescriptions;
    for (auto i = 0; i < typeDescriptions.size; i++)
    {
        isUnityEngineObject = false;
        TypeDescription &type = typeDescriptions[i];
        if (readTypeIndex(managedTypeIndex.unityengine_Object, type, &sUnityEngineObject))
        {
            isUnityEngineObject = true;
        }
        else if (readTypeIndex(managedTypeIndex.system_String, type, &sSystemString)) {}
        else if (readTypeIndex(managedTypeIndex.unityengine_TextGenerator, type, &sSystemTextGenerator)) {}
        
        Array<FieldDescription> &fieldDescriptions = *type.fields;
        for (auto n = 0; n < fieldDescriptions.size; n++)
        {
            FieldDescription &field = fieldDescriptions[n];
            if (isUnityEngineObject && endsWith(field.name, &sCachedPtr))
            {
                snapshot->cached_ptr = &field;
            }
            
            field.hookTypeIndex = type.typeIndex;
            field.slotIndex = n;
        }
    }
    sampler.end();
    
    sampler.begin("set_native_type_index");
    Array<PackedNativeType> &nativeTypes = *snapshot->nativeTypes;
    for (auto i = 0; i < nativeTypes.size; i++)
    {
        nativeTypes[i].typeIndex = i;
    }
    sampler.end();
    
    sampler.begin("set_gchandle_index");
    Array<PackedGCHandle> &gcHandles = *snapshot->gcHandles;
    for (auto i = 0; i < gcHandles.size; i++)
    {
        gcHandles[i].gcHandleArrayIndex = i;
    }
    sampler.end();
    
    sampler.begin("set_heap_index");
    Array<MemorySection> &managedHeapSections = *snapshot->managedHeapSections;
    for (auto i = 0; i < managedHeapSections.size; i++)
    {
        managedHeapSections[i].heapArrayIndex = i;
    }
    sampler.end();
    
    sampler.begin("set_native_object_index");
    Array<PackedNativeUnityEngineObject> &nativeObjects = *snapshot->nativeObjects;
    for (auto i = 0; i < nativeObjects.size; i++)
    {
        nativeObjects[i].nativeObjectArrayIndex = i;
    }
    sampler.end();
}

MemorySnapshotReader::~MemorySnapshotReader()
{
    sampler.summary();
    delete __fs;
}
