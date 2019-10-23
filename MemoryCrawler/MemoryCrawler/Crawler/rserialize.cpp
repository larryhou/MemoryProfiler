//
//  rserialize.cpp
//  MemoryCrawler
//
//  Created by LARRYHOU on 2019/10/23.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "rserialize.h"

const uint32_t kSnapshotMagicBytes = 0xFABCED01;
const uint32_t kSnapshotHeapMagicBytes = 0x9111DAAA;
const uint32_t kSnapshotStacksMagicBytes = 0x147358AA;
const uint32_t kSnapshotMetadataMagicBytes = 0x4891AEFD;
const uint32_t kSnapshotGCHandlesMagicBytes = 0x3456132C;
const uint32_t kSnapshotNativeTypesMagicBytes = 0x78514753;
const uint32_t kSnapshotNativeObjectsMagicBytes = 0x6173FAFE;
const uint32_t kSnapshotRuntimeInfoMagicBytes = 0x0183EFAC;
const uint32_t kSnapshotTailMagicBytes = 0x865EEAAF;

void readMemorySectionR(MemorySection &section, FileStream &fs)
{
    section.startAddress = fs.readUInt64();
    auto size = section.size = fs.readUInt32();
    if (size > 0)
    {
        section.bytes = new Array<byte_t>(size);
        fs.read((char *)(section.bytes->items), size);
    }
}

void readFieldDescriptionR(FieldDescription &item, FileStream &fs)
{
    item.offset = fs.readInt32();
    item.typeIndex = fs.readInt32();
    item.name = fs.readZEString();
    item.isStatic = fs.readBoolean();
}

void readTypeDescriptionR(TypeDescription &item, FileStream &fs)
{
    auto flags = fs.readInt32();
    item.isValueType = (flags & (1 << 0)) != 0;
    item.isArray = (flags & (1 << 1)) != 0;
    item.arrayRank = flags & 0xFFFF0000;
    item.baseOrElementTypeIndex = fs.readInt32();
    if (!item.isArray)
    {
        auto fieldCount = fs.readUInt32();
        item.fields = new Array<FieldDescription>(fieldCount);
        if (fieldCount > 0)
        {
            for (auto i = 0; i < fieldCount; i++)
            {
                readFieldDescriptionR(item.fields->items[i], fs);
            }
        }
        
        auto byteCount = fs.readUInt32();
        if (byteCount > 0)
        {
            item.staticFieldBytes = new Array<byte_t>(byteCount);
            fs.read((char *)(item.staticFieldBytes->items), byteCount);
        }
    }
    
    item.name = fs.readZEString();
    item.assembly = fs.readZEString();
    item.typeInfoAddress = fs.readUInt64();
    item.size = fs.readInt32();
}

RawMemorySnapshotReader::RawMemorySnapshotReader(const char *filepath): MemorySnapshotDeserializer(filepath) {}
RawMemorySnapshotReader::~RawMemorySnapshotReader()
{
    
}

void RawMemorySnapshotReader::read(PackedMemorySnapshot &snapshot)
{
    __sampler.begin("MemorySnapshotReader");
    __sampler.begin("open_snapshot");
    MemorySnapshotDeserializer::read(snapshot);
    __sampler.end();
    
    auto &fs = __fs;
    while (fs.byteAvailable())
    {
        auto magic = fs.readUInt32();
        switch (magic)
        {
            case kSnapshotMagicBytes:
            {
                formatVersion = fs.readUInt32();
            }break;
            case kSnapshotHeapMagicBytes:
            {
                __sampler.begin("readHeapMemorySections");
                auto sectionCount = fs.readUInt32();
                snapshot.heapSections = new Array<MemorySection>(sectionCount);
                for (auto i = 0; i < sectionCount; i++)
                {
                    readMemorySectionR(snapshot.heapSections->items[i], fs);
                }
                __sampler.end();
            }break;
            case kSnapshotStacksMagicBytes:
            {
                __sampler.begin("readStacksMemorySections");
                auto sectionCount = fs.readUInt32();
                snapshot.stacksSections = new Array<MemorySection>(sectionCount);
                for (auto i = 0; i < sectionCount; i++)
                {
                    readMemorySectionR(snapshot.stacksSections->items[i], fs);
                }
                __sampler.end();
            }break;
            case kSnapshotMetadataMagicBytes:
            {
                __sampler.begin("readTypeDescriptions");
                auto typeCount = fs.readUInt32();
                auto typeDescriptions = snapshot.typeDescriptions = new Array<TypeDescription>(typeCount);
                for (auto i = 0; i < typeCount; i++)
                {
                    auto &item = typeDescriptions->items[i];
                    readTypeDescriptionR(item, fs);
                    item.typeIndex = i;
                }
                __sampler.end();
            }break;
            case kSnapshotGCHandlesMagicBytes:
            {
                __sampler.begin("readGCHandles");
                auto itemCount = fs.readUInt32();
                auto gcHandles = snapshot.gcHandles = new Array<PackedGCHandle>(itemCount);
                for (auto i = 0; i < itemCount; i++)
                {
                    gcHandles->items[i].target = fs.readUInt64();
                }
                __sampler.end();
            }break;
            case kSnapshotNativeTypesMagicBytes:
            {
                __sampler.begin("readNativeTypes");
                auto typeCount = fs.readUInt32();
                
                if (formatVersion <= 3)
                {
                    auto maxClassID = 0;
                    for (auto i = 0; i < typeCount; i++)
                    {
                        auto classID = fs.readInt32();
                        fs.readInt32();
                        fs.readZEString();
                        if (maxClassID < classID) { maxClassID = classID; } 
                    }
                    typeCount = maxClassID + 1;
                }
                
                auto nativeTypes = snapshot.nativeTypes = new Array<PackedNativeType>(typeCount);
                for (auto i = 0; i < typeCount; i++)
                {
                    auto &type = nativeTypes->items[i];
                    type.typeIndex = fs.readInt32();
                    type.nativeBaseTypeArrayIndex = fs.readInt32();
                    type.name = fs.readZEString();
                }
                
                if (formatVersion <= 3)
                {
                    for (auto i = 0; i < typeCount; i++)
                    {
                        auto &type = nativeTypes->items[i];
                        if (type.name.empty())
                        {
                            type.nativeBaseTypeArrayIndex = -1;
                        }
                    }
                }
                __sampler.end();
            }break;
            case kSnapshotNativeObjectsMagicBytes:
            {
                __sampler.begin("readNativeObjectsAndConnections");
                std::vector<Connection> connections;
                auto gcHandleCount = snapshot.gcHandles->size;
                
                auto itemCount = fs.readUInt32();
                auto nativeObjects = snapshot.nativeObjects = new Array<PackedNativeUnityEngineObject>(itemCount);
                for (auto i = 0; i < itemCount; i++)
                {
                    auto &obj = nativeObjects->items[i];
                    obj.name = fs.readZEString();
                    obj.instanceId = fs.readInt32();
                    obj.size = fs.readInt32();
                    obj.nativeTypeArrayIndex = fs.readInt32();
                    obj.hideFlags = fs.readInt32();
                    obj.flags = fs.readInt32();
                    if (formatVersion > 2)
                    {
                        obj.nativeObjectAddress = fs.readUInt64();
                    }
                    
                    auto gcHandleIndex = fs.readInt32();
                    if (gcHandleIndex != -1)
                    {
                        Connection c;
                        c.from = gcHandleCount + i;
                        c.to = gcHandleIndex;
                        connections.emplace_back(c);
                    }
                    
                    auto referenceCount = fs.readUInt32();
                    while (referenceCount-- > 0)
                    {
                        auto referencedObjectIndex = fs.readInt32();
                        if (referencedObjectIndex != -1)
                        {
                            Connection c;
                            c.from = gcHandleCount + i;
                            c.to = gcHandleCount + referencedObjectIndex;
                            connections.emplace_back(c);
                        }
                    }
                }
                
                // create_connections
                snapshot.connections = new Array<Connection>((uint32_t)connections.size());
                if (connections.size() > 0)
                {
                    memcpy((char *)snapshot.connections->items, (const char *)(&connections.front()), sizeof(Connection) * connections.size());
                }
                __sampler.end();
            }break;
            case kSnapshotRuntimeInfoMagicBytes:
            {
                __sampler.begin("readVirtualMatchineInformation");
                auto &vm = snapshot.virtualMachineInformation;
                vm.pointerSize = fs.readInt32();
                vm.objectHeaderSize = fs.readInt32();
                vm.arrayHeaderSize = fs.readInt32();
                vm.arrayBoundsOffsetInHeader = fs.readInt32();
                vm.arraySizeOffsetInHeader = fs.readInt32();
                vm.allocationGranularity = fs.readInt32();
                assert(fs.readUInt32() == kSnapshotTailMagicBytes);
                __sampler.end();
            }break;
            default: break;
        }
    }
    
    uuid = "5241574d-454d-5341-5042-594c41525259"; // => RAWMEMSAPBYLARRY = RawMemorySnapshot by LARRYHOU
    prepareSnapshot();
    
    __sampler.end();
    #if PERF_DEBUG
        __sampler.summarize();
    #endif
}
