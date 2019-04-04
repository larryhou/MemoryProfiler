//
//  serialize.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/4.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "serialize.h"
#include <new>

static char buffer[8]; // warning: multi-threading

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
                break;
            
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

static const string sPackedMemorySnapshot("PackedMemorySnapshot");
static const string sVirtualMachineInformation("VirtualMachineInformation");
static const string sPackedNativeType("PackedNativeType");
static const string sPackedNativeUnityEngineObject("PackedNativeUnityEngineObject");
static const string sPackedGCHandle("PackedGCHandle");
static const string sMemorySection("MemorySection");
static const string sTypeDescription("TypeDescription");
static const string sFieldDescription("FieldDescription");
static const string sConnection("Connection");
static const string sArray("[]");
static const string sByteArray("Byte[]");

void readField(FileStream &fs)
{
    auto fieldName = fs.readString(swap(fs.readUInt32()));
    auto fieldType = fs.readString(swap(fs.readUInt32()));
    printf("%s %s\n", fieldType.c_str(), fieldName.c_str());
}

void readVirtualMachineInformation(VirtualMachineInformation &vm, FileStream &fs)
{
    auto fieldCount = fs.readUInt8();
    readField(fs);
    vm.pointerSize = swap(fs.readInt32());
    readField(fs);
    vm.objectHeaderSize = swap(fs.readInt32());
    readField(fs);
    vm.arrayHeaderSize = swap(fs.readInt32());
    readField(fs);
    vm.arrayBoundsOffsetInHeader = swap(fs.readInt32());
    readField(fs);
    vm.arraySizeOffsetInHeader = swap(fs.readInt32());
    readField(fs);
    vm.allocationGranularity = swap(fs.readInt32());
    readField(fs);
    vm.heapFormatVersion = swap(fs.readInt32());
}

void MemorySnapshotReader::readSnapshot(FileStream &fs)
{
    readObject(fs);
}

void MemorySnapshotReader::readObject(FileStream &fs)
{
    auto classType = fs.readString(swap(fs.readUInt32()));
    if (endsWith(&classType, &sVirtualMachineInformation))
    {
        vm = new VirtualMachineInformation;
        readVirtualMachineInformation(*vm, fs);
    }
}

void MemorySnapshotReader::readArray(FileStream &fs)
{
    
}

MemorySnapshotReader::~MemorySnapshotReader()
{
    delete fs;
}
