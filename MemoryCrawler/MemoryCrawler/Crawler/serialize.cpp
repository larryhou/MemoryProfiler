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

static char buffer[8]; // warning: multi-threading
TimeSampler<> profiler;

template <typename T>
T swap(T &value, int size)
{
    T *v = new(buffer) T;
    auto ptr = (char *)&value;
    for (auto i = 0; i < size; i++)
    {
        buffer[i] = ptr[size - i - 1];
    }
    
    return *v;
}

int16_t swap(int16_t value) { return swap<int16_t>(value, 2); }
int32_t swap(int32_t value) { return swap<int32_t>(value, 4); }
int64_t swap(int64_t value) { return swap<int64_t>(value, 8); }

uint16_t swap(uint16_t value) { return swap<uint16_t>(value, 2); }
uint32_t swap(uint32_t value) { return swap<uint32_t>(value, 4); }
uint64_t swap(uint64_t value) { return swap<uint64_t>(value, 8); }

double swap(double value) { return swap<double>(value, 8); }
float swap(float value) { return swap<float>(value, 4); }

void MemorySnapshotReader::read(const char *filepath)
{
    fs = new FileStream;
    fs->open(filepath);
    readHeader(*fs);
    while (fs->byteAvailable())
    {
        auto offset = fs->tell();
        auto length = swap(fs->readUInt32());
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
    mime = new string(fs.readString(3));
    description = new string(fs.readString(swap(fs.readUInt32())));
    unityVersion = new string(fs.readString(swap(fs.readUInt32())));
    systemVersion = new string(fs.readString(swap(fs.readUInt32())));
    uuid = new string(fs.readUUID());
    size = swap(fs.readUInt32());
    createTime = swap(fs.readUInt64());
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
    auto fieldName = fs.readString(swap(fs.readUInt32()));
    auto fieldType = fs.readString(swap(fs.readUInt32()));
//    printf("%s %s\n", fieldType.c_str(), fieldName.c_str());
}

//MARK: read object
static const string sPackedNativeUnityEngineObject("PackedNativeUnityEngineObject");
void readPackedNativeUnityEngineObject(PackedNativeUnityEngineObject &item, FileStream &fs)
{
    profiler.begin("readPackedNativeUnityEngineObject");
    profiler.begin("readType");
    auto classType = fs.readString(swap(fs.readUInt32()));
    assert(endsWith(&classType, &sPackedNativeUnityEngineObject));
    
    profiler.end();
    
    profiler.begin("readFields");
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 10);
    
    readField(fs);
    item.isPersistent = swap(fs.readBoolean());
    readField(fs);
    item.isDontDestroyOnLoad = swap(fs.readBoolean());
    readField(fs);
    item.isManager = swap(fs.readBoolean());
    readField(fs);
    item.name = new string(fs.readString(swap(fs.readUInt32())));
    readField(fs);
    item.instanceId = swap(fs.readInt32());
    readField(fs);
    item.size = swap(fs.readInt32());
    readField(fs);
    item.classId = swap(fs.readInt32());
    readField(fs);
    item.nativeTypeArrayIndex = swap(fs.readInt32());
    readField(fs);
    item.hideFlags = swap(fs.readUInt32());
    readField(fs);
    item.nativeObjectAddress = swap(fs.readInt64());
    profiler.end();
    profiler.end();
}

static const string sPackedNativeType("PackedNativeType");
void readPackedNativeType(PackedNativeType &item, FileStream &fs)
{
    profiler.begin("readPackedNativeType");
    profiler.begin("readType");
    auto classType = fs.readString(swap(fs.readUInt32()));
    assert(endsWith(&classType, &sPackedNativeType));
    
    profiler.end();
    
    profiler.begin("readFields");
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 3);
    
    readField(fs);
    item.name = new string(fs.readString(swap(fs.readUInt32())));
    readField(fs);
    item.baseClassId = swap(fs.readInt32());
    readField(fs);
    item.nativeBaseTypeArrayIndex = swap(fs.readInt32());
    profiler.end();
    profiler.end();
}

static const string sPackedGCHandle("PackedGCHandle");
void readPackedGCHandle(PackedGCHandle &item, FileStream &fs)
{
    profiler.begin("readPackedGCHandle");
    profiler.begin("readType");
    auto classType = fs.readString(swap(fs.readUInt32()));
    assert(endsWith(&classType, &sPackedGCHandle));
    
    profiler.end();
    
    profiler.begin("readFields");
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 1);
    
    readField(fs);
    item.target = swap(fs.readUInt64());
    profiler.end();
    profiler.end();
}

static const string sConnection("Connection");
void readConnection(Connection &item, FileStream &fs)
{
    profiler.begin("readConnection");
    profiler.begin("readType");
    auto classType = fs.readString(swap(fs.readUInt32()));
    assert(endsWith(&classType, &sConnection));
    
    profiler.end();
    
    profiler.begin("readFields");
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 2);
    
    readField(fs);
    item.from = swap(fs.readInt32());
    readField(fs);
    item.to = swap(fs.readInt32());
    profiler.end();
    profiler.end();
}

static const string sMemorySection("MemorySection");
void readMemorySection(MemorySection &item, FileStream &fs)
{
    profiler.begin("readMemorySection");
    profiler.begin("readType");
    auto classType = fs.readString(swap(fs.readUInt32()));
    assert(endsWith(&classType, &sMemorySection));
    
    profiler.end();
    
    profiler.begin("readFields");
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 2);
    
    readField(fs);
    {
        auto size = swap(fs.readUInt32());
        char *data = new char[size];
        fs.read(data, size);
        item.bytes = (unsigned char *)data;
    }
    readField(fs);
    item.startAddress = swap(fs.readUInt64());
    profiler.end();
    profiler.end();
}

static const string sFieldDescription("FieldDescription");
void readFieldDescription(FieldDescription &item, FileStream &fs)
{
    profiler.begin("readFieldDescription");
    profiler.begin("readType");
    auto classType = fs.readString(swap(fs.readUInt32()));
    assert(endsWith(&classType, &sFieldDescription));
    
    profiler.end();
    
    profiler.begin("readFields");
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 4);
    
    readField(fs);
    item.name = new string(fs.readString(swap(fs.readUInt32())));
    readField(fs);
    item.offset = swap(fs.readInt32());
    readField(fs);
    item.typeIndex = swap(fs.readInt32());
    readField(fs);
    item.isStatic = swap(fs.readBoolean());
    profiler.end();
    profiler.end();
}

static const string sTypeDescription("TypeDescription");
void readTypeDescription(TypeDescription &item, FileStream &fs)
{
    profiler.begin("readTypeDescription");
    profiler.begin("readType");
    auto classType = fs.readString(swap(fs.readUInt32()));
    assert(endsWith(&classType, &sTypeDescription));
    
    profiler.end();
    
    profiler.begin("readFields");
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 11);
    
    readField(fs);
    item.isValueType = swap(fs.readBoolean());
    readField(fs);
    item.isArray = swap(fs.readBoolean());
    readField(fs);
    item.arrayRank = swap(fs.readInt32());
    readField(fs);
    item.name = new string(fs.readString(swap(fs.readUInt32())));
    readField(fs);
    item.assembly = new string(fs.readString(swap(fs.readUInt32())));
    readField(fs);
    {
        auto size = swap(fs.readUInt32());
        item.fields = new FieldDescription[size];
        for (auto i = 0; i < size; i++)
        {
            readFieldDescription(item.fields[i], fs);
        }
    }
    readField(fs);
    {
        auto size = swap(fs.readUInt32());
        char *data = new char[size];
        fs.read(data, size);
        item.staticFieldBytes = (unsigned char *)data;
    }
    readField(fs);
    item.baseOrElementTypeIndex = swap(fs.readInt32());
    readField(fs);
    item.size = swap(fs.readInt32());
    readField(fs);
    item.typeInfoAddress = swap(fs.readUInt64());
    readField(fs);
    item.typeIndex = swap(fs.readInt32());
    profiler.end();
    profiler.end();
}

static const string sVirtualMachineInformation("VirtualMachineInformation");
void readVirtualMachineInformation(VirtualMachineInformation &item, FileStream &fs)
{
    profiler.begin("readVirtualMachineInformation");
    profiler.begin("readType");
    auto classType = fs.readString(swap(fs.readUInt32()));
    assert(endsWith(&classType, &sVirtualMachineInformation));
    
    profiler.end();
    
    profiler.begin("readFields");
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 7);
    
    readField(fs);
    item.pointerSize = swap(fs.readInt32());
    readField(fs);
    item.objectHeaderSize = swap(fs.readInt32());
    readField(fs);
    item.arrayHeaderSize = swap(fs.readInt32());
    readField(fs);
    item.arrayBoundsOffsetInHeader = swap(fs.readInt32());
    readField(fs);
    item.arraySizeOffsetInHeader = swap(fs.readInt32());
    readField(fs);
    item.allocationGranularity = swap(fs.readInt32());
    readField(fs);
    item.heapFormatVersion = swap(fs.readInt32());
    profiler.end();
    profiler.end();
}

static const string sPackedMemorySnapshot("PackedMemorySnapshot");
void readPackedMemorySnapshot(PackedMemorySnapshot &item, FileStream &fs)
{
    profiler.begin("readPackedMemorySnapshot");
    profiler.begin("readType");
    auto classType = fs.readString(swap(fs.readUInt32()));
    assert(endsWith(&classType, &sPackedMemorySnapshot));
    
    profiler.end();
    
    profiler.begin("readFields");
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 7);
    
    readField(fs);
    {
        auto size = swap(fs.readUInt32());
        item.nativeTypes = new PackedNativeType[size];
        for (auto i = 0; i < size; i++)
        {
            readPackedNativeType(item.nativeTypes[i], fs);
        }
    }
    readField(fs);
    {
        auto size = swap(fs.readUInt32());
        item.nativeObjects = new PackedNativeUnityEngineObject[size];
        for (auto i = 0; i < size; i++)
        {
            readPackedNativeUnityEngineObject(item.nativeObjects[i], fs);
        }
    }
    readField(fs);
    {
        auto size = swap(fs.readUInt32());
        item.gcHandles = new PackedGCHandle[size];
        for (auto i = 0; i < size; i++)
        {
            readPackedGCHandle(item.gcHandles[i], fs);
        }
    }
    readField(fs);
    {
        auto size = swap(fs.readUInt32());
        item.connections = new Connection[size];
        for (auto i = 0; i < size; i++)
        {
            readConnection(item.connections[i], fs);
        }
    }
    readField(fs);
    {
        auto size = swap(fs.readUInt32());
        item.managedHeapSections = new MemorySection[size];
        for (auto i = 0; i < size; i++)
        {
            readMemorySection(item.managedHeapSections[i], fs);
        }
    }
    readField(fs);
    {
        auto size = swap(fs.readUInt32());
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
