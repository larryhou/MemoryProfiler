//
//  serialize.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/4.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "serialize.h"
#include "perf.h"
#include <new>

static TimeSampler<std::micro> sampler;

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
        char *data = new char[size];
        fs.read(data, size);
        item.bytes = (unsigned char *)data;
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
        item.fields = new FieldDescription[size];
        for (auto i = 0; i < size; i++)
        {
            readFieldDescription(item.fields[i], fs);
        }
    }
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        char *data = new char[size];
        fs.read(data, size);
        item.staticFieldBytes = (unsigned char *)data;
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
    sampler.begin("readVirtualMachineInformation");
    sampler.begin("read_type");
    fs.skipString(true);
    sampler.end();
    sampler.begin("read_field_count");
    auto fieldCount = fs.readUInt8();
    sampler.end();
    sampler.begin("assert field_count");
    assert(fieldCount == 7);
    sampler.end();
    
    readField(fs);
    sampler.begin("read_pointer_size");
    item.pointerSize = fs.readInt32(true);
    sampler.end();
    
    readField(fs);
    sampler.begin("read_object_header_size");
    item.objectHeaderSize = fs.readInt32(true);
    sampler.end();
    
    readField(fs);
    sampler.begin("read_array_header_size");
    item.arrayHeaderSize = fs.readInt32(true);
    sampler.end();
    
    readField(fs);
    sampler.begin("read_array_bounds_offset_in_header");
    item.arrayBoundsOffsetInHeader = fs.readInt32(true);
    sampler.end();
    
    readField(fs);
    sampler.begin("read_array_size_offset_in_header");
    item.arraySizeOffsetInHeader = fs.readInt32(true);
    sampler.end();
    
    readField(fs);
    sampler.begin("read_allocation_granularity");
    item.allocationGranularity = fs.readInt32(true);
    sampler.end();
    
    readField(fs);
    sampler.begin("read_heap_format_version");
    item.heapFormatVersion = fs.readInt32(true);
    sampler.end();
    sampler.end();
}

static const string sPackedMemorySnapshot("PackedMemorySnapshot");
void readPackedMemorySnapshot(PackedMemorySnapshot &item, FileStream &fs)
{
    sampler.begin("readPackedMemorySnapshot");
    sampler.begin("read_type");
    fs.skipString(true);
    sampler.end();
    sampler.begin("read_field_count");
    auto fieldCount = fs.readUInt8();
    sampler.end();
    sampler.begin("assert_field_count");
    assert(fieldCount == 7);
    sampler.end();
    
    sampler.begin("read_native_types");
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        item.nativeTypes = new PackedNativeType[size];
        for (auto i = 0; i < size; i++)
        {
            readPackedNativeType(item.nativeTypes[i], fs);
        }
    }
    sampler.end();
    sampler.begin("read_native_objects");
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        item.nativeObjects = new PackedNativeUnityEngineObject[size];
        for (auto i = 0; i < size; i++)
        {
            readPackedNativeUnityEngineObject(item.nativeObjects[i], fs);
        }
    }
    sampler.end();
    sampler.begin("read_gc_handles");
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        item.gcHandles = new PackedGCHandle[size];
        for (auto i = 0; i < size; i++)
        {
            readPackedGCHandle(item.gcHandles[i], fs);
        }
    }
    sampler.end();
    sampler.begin("read_connections");
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        
        item.connections = new Connection[1];
        
        auto offset = fs.tell();
        readConnection(item.connections[0], fs);
        auto length = fs.tell() - offset;
        
        fs.ignore(length * (size - 1));
    }
    sampler.end();
    sampler.begin("read_managed_heap_sections");
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        item.managedHeapSections = new MemorySection[size];
        for (auto i = 0; i < size; i++)
        {
            readMemorySection(item.managedHeapSections[i], fs);
        }
    }
    sampler.end();
    sampler.begin("read_type_descriptions");
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        item.typeDescriptions = new TypeDescription[size];
        for (auto i = 0; i < size; i++)
        {
            readTypeDescription(item.typeDescriptions[i], fs);
        }
    }
    sampler.end();
    readField(fs);
    sampler.begin("read_virtual_machine_information");
    item.virtualMachineInformation = new VirtualMachineInformation;
    readVirtualMachineInformation(*item.virtualMachineInformation, fs);
    sampler.end();
    sampler.end();
}

void MemorySnapshotReader::readSnapshot(FileStream &fs)
{
    vm = new VirtualMachineInformation;
    readVirtualMachineInformation(*vm, fs);
    
    snapshot = new PackedMemorySnapshot;
    readPackedMemorySnapshot(*snapshot, fs);
}

MemorySnapshotReader::~MemorySnapshotReader()
{
    sampler.summary();
    delete __fs;
}
