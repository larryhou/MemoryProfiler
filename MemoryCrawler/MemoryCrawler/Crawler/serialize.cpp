//
//  serialize.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/4.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "serialize.h"
#include <map>

MemorySnapshotReader::MemorySnapshotReader(const char *filepath)
{
    auto size = strlen(filepath);
    auto buffer = new char[size];
    std::strcpy(buffer, filepath);
    __filepath = buffer;
}

PackedMemorySnapshot &MemorySnapshotReader::read(PackedMemorySnapshot &snapshot)
{
    this->__snapshot = &snapshot;
    __sampler.begin("MemorySnapshotReader");
    if (__fs == nullptr)
    {
        __fs = new FileStream;
        __sampler.begin("open_snapshot");
        __fs->open(__filepath, false);
        __sampler.end();
    }
    else
    {
        __fs->seek(0, seekdir_t::beg);
    }
    
    __sampler.begin("read_header");
    readHeader(*__fs);
    __sampler.end();
    assert(__fs->tell() == 124);
    while (__fs->byteAvailable())
    {
        auto length = __fs->readUInt32(true);
        auto type = __fs->readUInt8();
        switch (type)
        {
            case '0':
                readSnapshot(*__fs);
                break;
            
            default:
                __fs->ignore(length - 5);
                break;
        }
        __fs->ignore(8);
    }
    __sampler.end();
    __sampler.summary();
    return snapshot;
}

void MemorySnapshotReader::readHeader(FileStream &fs)
{
    mime = new string(fs.readString((size_t)3));
    assert(*mime == string("PMS"));
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
        auto size = fs.readInt32(true);
        item.fields = new Array<FieldDescription>(size);
        for (auto i = 0; i < size; i++)
        {
            readFieldDescription(item.fields->items[i], fs);
        }
    }
    readField(fs);
    {
        auto size = fs.readInt32(true);
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
void MemorySnapshotReader::readPackedMemorySnapshot(PackedMemorySnapshot &item, FileStream &fs)
{
    __sampler.begin("readPackedMemorySnapshot");
    __sampler.begin("read_type_string");
    fs.skipString(true);
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 7);
    __sampler.end();
    
    readField(fs);
    {
        __sampler.begin("read_native_types");
        auto size = fs.readUInt32(true);
        item.nativeTypes = new Array<PackedNativeType>(size);
        for (auto i = 0; i < size; i++)
        {
            readPackedNativeType(item.nativeTypes->items[i], fs);
        }
        __sampler.end();
    }
    readField(fs);
    {
        __sampler.begin("read_native_objects");
        auto size = fs.readUInt32(true);
        item.nativeObjects = new Array<PackedNativeUnityEngineObject>(size);
        for (auto i = 0; i < size; i++)
        {
            readPackedNativeUnityEngineObject(item.nativeObjects->items[i], fs);
        }
        __sampler.end();
    }
    readField(fs);
    {
        __sampler.begin("read_gc_handles");
        auto size = fs.readUInt32(true);
        item.gcHandles = new Array<PackedGCHandle>(size);
        for (auto i = 0; i < size; i++)
        {
            readPackedGCHandle(item.gcHandles->items[i], fs);
        }
        __sampler.end();
    }
    readField(fs);
    {
        __sampler.begin("read_connections");
        auto size = fs.readUInt32(true);
        item.connections = new Array<Connection>(size);
        for (auto i = 0; i < size; i++)
        {
            readConnection(item.connections->items[i], fs);
        }
        __sampler.end();
    }
    readField(fs);
    {
        __sampler.begin("read_heap_sections");
        auto size = fs.readUInt32(true);
        item.managedHeapSections = new Array<MemorySection>(size);
        for (auto i = 0; i < size; i++)
        {
            readMemorySection(item.managedHeapSections->items[i], fs);
        }
        __sampler.end();
    }
    readField(fs);
    {
        __sampler.begin("read_type_descriptions");
        auto size = fs.readUInt32(true);
        item.typeDescriptions = new Array<TypeDescription>(size);
        for (auto i = 0; i < size; i++)
        {
            readTypeDescription(item.typeDescriptions->items[i], fs);
        }
        __sampler.end();
    }
    __sampler.begin("read_virtual_matchine_information");
    __sampler.begin("read_field_strings");
    readField(fs);
    __sampler.end();
    __sampler.begin("read_object");
    readVirtualMachineInformation(item.virtualMachineInformation, fs);
    __sampler.end();
    __sampler.end();
    __sampler.end();
}

void MemorySnapshotReader::readSnapshot(FileStream &fs)
{
    __vm = new VirtualMachineInformation;
    readVirtualMachineInformation(*__vm, fs);
    readPackedMemorySnapshot(*__snapshot, fs);
    
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
    __snapshot->uuid = new std::string(*uuid);
    
    __sampler.begin("postSnapshot");
    __sampler.begin("create_sorted_heap");
    for (auto i = 0; i < __snapshot->managedHeapSections->size; i++)
    {
        MemorySection &heap = __snapshot->managedHeapSections->items[i];
        heap.size = heap.bytes->size;
    }
    __sampler.end();
    
    __sampler.begin("create_type_strings");
    string sUnityEngineObject("UnityEngine.Object");
    string sSystemString("System.String");
    string sSystemTextGenerator("UnityEngine.TextGenerator");
    string sCachedPtr("m_CachedPtr");
    __sampler.end();
    
    ManagedTypeIndex &managedTypeIndex = __snapshot->managedTypeIndex;
    
    __sampler.begin("read_type_index");
    bool isUnityEngineObject = false;
    Array<TypeDescription> &typeDescriptions = *__snapshot->typeDescriptions;
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
        
        if (type.fields != nullptr)
        {
            Array<FieldDescription> &fieldDescriptions = *type.fields;
            for (auto n = 0; n < fieldDescriptions.size; n++)
            {
                FieldDescription &field = fieldDescriptions[n];
                if (isUnityEngineObject && endsWith(field.name, &sCachedPtr))
                {
                    __snapshot->cached_ptr = &field;
                }
                
                field.hookTypeIndex = type.typeIndex;
                field.fieldSlotIndex = n;
            }
        }
    }
    __cachedPtr = __snapshot->cached_ptr;
    assert(__cachedPtr != nullptr);
    __sampler.end();
    
    __sampler.begin("set_native_type_index");
    Array<PackedNativeType> &nativeTypes = *__snapshot->nativeTypes;
    for (auto i = 0; i < nativeTypes.size; i++)
    {
        nativeTypes[i].typeIndex = i;
    }
    __sampler.end();
    
    __sampler.begin("set_gchandle_index");
    Array<PackedGCHandle> &gcHandles = *__snapshot->gcHandles;
    for (auto i = 0; i < gcHandles.size; i++)
    {
        gcHandles[i].gcHandleArrayIndex = i;
    }
    __sampler.end();
    
    __sampler.begin("set_heap_index");
    Array<MemorySection> &managedHeapSections = *__snapshot->managedHeapSections;
    
    auto sortedHeapSections = new std::vector<MemorySection *>;
    for (auto i = 0; i < managedHeapSections.size; i++)
    {
        sortedHeapSections->push_back(&managedHeapSections[i]);
    }
    std::sort(sortedHeapSections->begin(), sortedHeapSections->end(), [](const MemorySection *a, const MemorySection *b)
              {
                  return a->startAddress < b->startAddress;
              });
    __snapshot->sortedHeapSections = sortedHeapSections;
    for (auto i = 0; i < sortedHeapSections->size(); i++)
    {
        (*sortedHeapSections)[i]->heapArrayIndex = i;
    }
    __sampler.end();
    
    __sampler.begin("set_native_object_index");
    Array<PackedNativeUnityEngineObject> &nativeObjects = *__snapshot->nativeObjects;
    for (auto i = 0; i < nativeObjects.size; i++)
    {
        nativeObjects[i].nativeObjectArrayIndex = i;
    }
    __sampler.end(); // set_native_object_index
    
    summarize();
    
    __sampler.end();
}

void MemorySnapshotReader::summarize()
{
    __sampler.begin("summarize_native_objects");
    
    auto &nativeTypes = *__snapshot->nativeTypes;
    for (auto i = 0; i < nativeTypes.size; i++)
    {
        auto &type = nativeTypes[i];
        type.instanceMemory = 0;
        type.instanceCount = 0;
    }
    
    auto &nativeObjects = *__snapshot->nativeObjects;
    for (auto i = 0; i < nativeObjects.size; i++)
    {
        auto &no = nativeObjects[i];
        auto &type = nativeTypes[no.nativeTypeArrayIndex];
        type.instanceMemory += no.size;
        type.instanceCount += 1;
    }
    
    __sampler.end();
}

MemorySnapshotReader::~MemorySnapshotReader()
{
    delete __fs;
    delete [] __filepath;
    delete mime;
    delete unityVersion;
    delete description;
    delete systemVersion;
    delete uuid;
}
