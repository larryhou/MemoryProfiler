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
static TimeSampler<> sampler;

static const string sPackedNativeUnityEngineObject("PackedNativeUnityEngineObject");
void readPackedNativeUnityEngineObject(PackedNativeUnityEngineObject &item, FileStream &fs)
{
    sampler.begin("readPackedNativeUnityEngineObject");
    sampler.begin("readType");
    auto classType = fs.readString(true);
    assert(endsWith(&classType, &sPackedNativeUnityEngineObject));
    
    sampler.end();
    
    sampler.begin("readFields");
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
    sampler.end();
    sampler.end();
}

static const string sPackedNativeType("PackedNativeType");
void readPackedNativeType(PackedNativeType &item, FileStream &fs)
{
    sampler.begin("readPackedNativeType");
    sampler.begin("readType");
    auto classType = fs.readString(true);
    assert(endsWith(&classType, &sPackedNativeType));
    
    sampler.end();
    
    sampler.begin("readFields");
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 3);
    
    readField(fs);
    item.name = new string(fs.readString(true));
    readField(fs);
    item.baseClassId = fs.readInt32(true);
    readField(fs);
    item.nativeBaseTypeArrayIndex = fs.readInt32(true);
    sampler.end();
    sampler.end();
}

static const string sPackedGCHandle("PackedGCHandle");
void readPackedGCHandle(PackedGCHandle &item, FileStream &fs)
{
    sampler.begin("readPackedGCHandle");
    sampler.begin("readType");
    auto classType = fs.readString(true);
    assert(endsWith(&classType, &sPackedGCHandle));
    
    sampler.end();
    
    sampler.begin("readFields");
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 1);
    
    readField(fs);
    item.target = fs.readUInt64(true);
    sampler.end();
    sampler.end();
}

static const string sConnection("Connection");
void readConnection(Connection &item, FileStream &fs)
{
    sampler.begin("readConnection");
    sampler.begin("readType");
    auto classType = fs.readString(true);
    assert(endsWith(&classType, &sConnection));
    
    sampler.end();
    
    sampler.begin("readFields");
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 2);
    
    readField(fs);
    item.from = fs.readInt32(true);
    readField(fs);
    item.to = fs.readInt32(true);
    sampler.end();
    sampler.end();
}

static const string sMemorySection("MemorySection");
void readMemorySection(MemorySection &item, FileStream &fs)
{
    sampler.begin("readMemorySection");
    sampler.begin("readType");
    auto classType = fs.readString(true);
    assert(endsWith(&classType, &sMemorySection));
    
    sampler.end();
    
    sampler.begin("readFields");
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
    sampler.end();
    sampler.end();
}

static const string sFieldDescription("FieldDescription");
void readFieldDescription(FieldDescription &item, FileStream &fs)
{
    sampler.begin("readFieldDescription");
    sampler.begin("readType");
    auto classType = fs.readString(true);
    assert(endsWith(&classType, &sFieldDescription));
    
    sampler.end();
    
    sampler.begin("readFields");
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
    sampler.end();
    sampler.end();
}

static const string sTypeDescription("TypeDescription");
void readTypeDescription(TypeDescription &item, FileStream &fs)
{
    sampler.begin("readTypeDescription");
    sampler.begin("readType");
    auto classType = fs.readString(true);
    assert(endsWith(&classType, &sTypeDescription));
    
    sampler.end();
    
    sampler.begin("readFields");
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
        sampler.begin("fields:FieldDescription[]");
        item.fields = new FieldDescription[size];
        for (auto i = 0; i < size; i++)
        {
            readFieldDescription(item.fields[i], fs);
        }
        sampler.end();
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
    sampler.end();
    sampler.end();
}

static const string sVirtualMachineInformation("VirtualMachineInformation");
void readVirtualMachineInformation(VirtualMachineInformation &item, FileStream &fs)
{
    sampler.begin("readVirtualMachineInformation");
    sampler.begin("readType");
    auto classType = fs.readString(true);
    assert(endsWith(&classType, &sVirtualMachineInformation));
    
    sampler.end();
    
    sampler.begin("readFields");
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
    sampler.end();
    sampler.end();
}

static const string sPackedMemorySnapshot("PackedMemorySnapshot");
void readPackedMemorySnapshot(PackedMemorySnapshot &item, FileStream &fs)
{
    sampler.begin("readPackedMemorySnapshot");
    sampler.begin("readType");
    auto classType = fs.readString(true);
    assert(endsWith(&classType, &sPackedMemorySnapshot));
    
    sampler.end();
    
    sampler.begin("readFields");
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 7);
    
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        sampler.begin("nativeTypes:PackedNativeType[]");
        item.nativeTypes = new PackedNativeType[size];
        for (auto i = 0; i < size; i++)
        {
            readPackedNativeType(item.nativeTypes[i], fs);
        }
        sampler.end();
    }
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        sampler.begin("nativeObjects:PackedNativeUnityEngineObject[]");
        item.nativeObjects = new PackedNativeUnityEngineObject[size];
        for (auto i = 0; i < size; i++)
        {
            readPackedNativeUnityEngineObject(item.nativeObjects[i], fs);
        }
        sampler.end();
    }
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        sampler.begin("gcHandles:PackedGCHandle[]");
        item.gcHandles = new PackedGCHandle[size];
        for (auto i = 0; i < size; i++)
        {
            readPackedGCHandle(item.gcHandles[i], fs);
        }
        sampler.end();
    }
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        sampler.begin("connections:Connection[]");
        item.connections = new Connection[size];
        for (auto i = 0; i < size; i++)
        {
            readConnection(item.connections[i], fs);
        }
        sampler.end();
    }
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        sampler.begin("managedHeapSections:MemorySection[]");
        item.managedHeapSections = new MemorySection[size];
        for (auto i = 0; i < size; i++)
        {
            readMemorySection(item.managedHeapSections[i], fs);
        }
        sampler.end();
    }
    readField(fs);
    {
        auto size = fs.readUInt32(true);
        sampler.begin("typeDescriptions:TypeDescription[]");
        item.typeDescriptions = new TypeDescription[size];
        for (auto i = 0; i < size; i++)
        {
            readTypeDescription(item.typeDescriptions[i], fs);
        }
        sampler.end();
    }
    readField(fs);
    item.virtualMachineInformation = new VirtualMachineInformation;
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
    delete fs;
}
