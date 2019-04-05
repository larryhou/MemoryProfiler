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

TimeSampler<> profiler;

void MemorySnapshotReader::read(const char *filepath)
{
    fs = new FileStream;
    fs->open(filepath);
    readHeader(*fs);
    while (fs->byteAvailable())
    {
        auto offset = fs->tell();
        auto length = fs->readUInt32(true);
        auto type = fs->readUInt8();
        switch (type)
        {
            case '0':
                readSnapshot(*fs);
                return;
            
            default:
                fs->seek(offset + length, seekdir_t::beg);
                break;
        }
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
    auto fieldName = fs.readString(true);
    auto fieldType = fs.readString(true);
//    printf("%s %s\n", fieldType.c_str(), fieldName.c_str());
}

//MARK: read object
static const string sPackedNativeUnityEngineObject("PackedNativeUnityEngineObject");
void readPackedNativeUnityEngineObject(PackedNativeUnityEngineObject &item, FileStream &fs)
{
    profiler.begin("readPackedNativeUnityEngineObject");
    profiler.begin("readType");
    auto classType = fs.readString(true);
    assert(endsWith(&classType, &sPackedNativeUnityEngineObject));
    
    profiler.end();
    
    profiler.begin("readFields");
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
    profiler.end();
    profiler.end();
}

static const string sPackedNativeType("PackedNativeType");
void readPackedNativeType(PackedNativeType &item, FileStream &fs)
{
    profiler.begin("readPackedNativeType");
    profiler.begin("readType");
    auto classType = fs.readString(true);
    assert(endsWith(&classType, &sPackedNativeType));
    
    profiler.end();
    
    profiler.begin("readFields");
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 3);
    
    readField(fs);
    item.name = new string(fs.readString(true));
    readField(fs);
    item.baseClassId = fs.readInt32(true);
    readField(fs);
    item.nativeBaseTypeArrayIndex = fs.readInt32(true);
    profiler.end();
    profiler.end();
}

static const string sPackedGCHandle("PackedGCHandle");
void readPackedGCHandle(PackedGCHandle &item, FileStream &fs)
{
    profiler.begin("readPackedGCHandle");
    profiler.begin("readType");
    auto classType = fs.readString(true);
    assert(endsWith(&classType, &sPackedGCHandle));
    
    profiler.end();
    
    profiler.begin("readFields");
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 1);
    
    readField(fs);
    item.target = fs.readUInt64(true);
    profiler.end();
    profiler.end();
}

static const string sConnection("Connection");
void readConnection(Connection &item, FileStream &fs)
{
    profiler.begin("readConnection");
    profiler.begin("readType");
    auto classType = fs.readString(true);
    assert(endsWith(&classType, &sConnection));
    
    profiler.end();
    
    profiler.begin("readFields");
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 2);
    
    readField(fs);
    item.from = fs.readInt32(true);
    readField(fs);
    item.to = fs.readInt32(true);
    profiler.end();
    profiler.end();
}

static const string sMemorySection("MemorySection");
void readMemorySection(MemorySection &item, FileStream &fs)
{
    profiler.begin("readMemorySection");
    profiler.begin("readType");
    auto classType = fs.readString(true);
    assert(endsWith(&classType, &sMemorySection));
    
    profiler.end();
    
    profiler.begin("readFields");
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
    profiler.end();
    profiler.end();
}

static const string sFieldDescription("FieldDescription");
void readFieldDescription(FieldDescription &item, FileStream &fs)
{
    profiler.begin("readFieldDescription");
    profiler.begin("readType");
    auto classType = fs.readString(true);
    assert(endsWith(&classType, &sFieldDescription));
    
    profiler.end();
    
    profiler.begin("readFields");
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
    profiler.end();
    profiler.end();
}

static const string sTypeDescription("TypeDescription");
void readTypeDescription(TypeDescription &item, FileStream &fs)
{
    profiler.begin("readTypeDescription");
    profiler.begin("readType");
    auto classType = fs.readString(true);
    assert(endsWith(&classType, &sTypeDescription));
    
    profiler.end();
    
    profiler.begin("readFields");
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
    profiler.end();
    profiler.end();
}

static const string sVirtualMachineInformation("VirtualMachineInformation");
void readVirtualMachineInformation(VirtualMachineInformation &item, FileStream &fs)
{
    profiler.begin("readVirtualMachineInformation");
    profiler.begin("readType");
    auto classType = fs.readString(true);
    assert(endsWith(&classType, &sVirtualMachineInformation));
    
    profiler.end();
    
    profiler.begin("readFields");
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
    profiler.end();
    profiler.end();
}

static const string sPackedMemorySnapshot("PackedMemorySnapshot");
void readPackedMemorySnapshot(PackedMemorySnapshot &item, FileStream &fs)
{
    profiler.begin("readPackedMemorySnapshot");
    profiler.begin("readType");
    auto classType = fs.readString(true);
    assert(endsWith(&classType, &sPackedMemorySnapshot));
    
    profiler.end();
    
    profiler.begin("readFields");
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 7);
    
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        item.nativeTypes = new PackedNativeType[size];
        for (auto i = 0; i < size; i++)
        {
            readPackedNativeType(item.nativeTypes[i], fs);
        }
    }
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        item.nativeObjects = new PackedNativeUnityEngineObject[size];
        for (auto i = 0; i < size; i++)
        {
            readPackedNativeUnityEngineObject(item.nativeObjects[i], fs);
        }
    }
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        item.gcHandles = new PackedGCHandle[size];
        for (auto i = 0; i < size; i++)
        {
            readPackedGCHandle(item.gcHandles[i], fs);
        }
    }
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        item.connections = new Connection[size];
        for (auto i = 0; i < size; i++)
        {
            readConnection(item.connections[i], fs);
        }
    }
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        item.managedHeapSections = new MemorySection[size];
        for (auto i = 0; i < size; i++)
        {
            readMemorySection(item.managedHeapSections[i], fs);
        }
    }
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        item.typeDescriptions = new TypeDescription[size];
        for (auto i = 0; i < size; i++)
        {
            readTypeDescription(item.typeDescriptions[i], fs);
        }
    }
    readField(fs);
    item.virtualMachineInformation = new VirtualMachineInformation;
    profiler.end();
    profiler.end();
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
    profiler.summary();
    delete fs;
}
