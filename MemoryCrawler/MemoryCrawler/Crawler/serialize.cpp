//
//  serialize.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/4.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "serialize.h"
#include <map>

void MemorySnapshotDeserializer::read(PackedMemorySnapshot &snapshot)
{
    __snapshot = &snapshot;
    __fs.open(__filepath.c_str());
}

MemorySnapshotReader::MemorySnapshotReader(const char *filepath): MemorySnapshotDeserializer(filepath) {}
MemorySnapshotReader::~MemorySnapshotReader()
{
    
}

void MemorySnapshotReader::read(PackedMemorySnapshot &snapshot)
{
    __sampler.begin("MemorySnapshotReader");
    __sampler.begin("OpenSnapshot");
    MemorySnapshotDeserializer::read(snapshot);
    __sampler.end();
    
    __sampler.begin("ReadHeader");
    readHeader(__fs);
    __sampler.end();
    
    while (__fs.byteAvailable())
    {
        auto length = __fs.readUInt32();
        auto type = __fs.readUInt8();
        switch (type)
        {
            case '0':
                readSnapshot(__fs);
                break;
            
            default:
                __fs.ignore(length - 5);
                break;
        }
        __fs.ignore(8);
    }
    __sampler.end();
#if PERF_DEBUG
    __sampler.summarize();
#endif
}

void MemorySnapshotReader::readHeader(FileStream &fs)
{
    mime = fs.readString((size_t)3);
    assert(mime == string("PMS"));
    
    description = fs.readString();
    unityVersion = fs.readString();
    systemVersion = fs.readString();
    uuid = fs.readUUID();
    size = fs.readUInt32();
    createTime = fs.readUInt64();
}

//MARK: read object
void readPackedNativeUnityEngineObject(PackedNativeUnityEngineObject &item, FileStream &fs)
{
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 10);
    
    item.isPersistent = fs.readBoolean();
    item.isDontDestroyOnLoad = fs.readBoolean();
    item.isManager = fs.readBoolean();
    item.name = fs.readString();
    item.instanceId = fs.readInt32();
    item.size = fs.readInt32();
    item.classId = fs.readInt32();
    item.nativeTypeArrayIndex = fs.readInt32();
    item.hideFlags = fs.readUInt32();
    item.nativeObjectAddress = fs.readInt64();
}

void readPackedNativeType(PackedNativeType &item, FileStream &fs)
{
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 3);
    
    item.name = fs.readString();
    item.baseClassId = fs.readInt32();
    item.nativeBaseTypeArrayIndex = fs.readInt32();
}

void readPackedGCHandle(PackedGCHandle &item, FileStream &fs)
{
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 1);
    
    item.target = fs.readUInt64();
}

void readConnection(Connection &item, FileStream &fs)
{
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 2);
    
    item.from = fs.readInt32();
    item.to = fs.readInt32();
}

void readMemorySection(MemorySection &item, FileStream &fs)
{
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 2);
    
    {
        auto size = fs.readUInt32();
        if (size > 0)
        {
            item.bytes = new Array<byte_t>(size);
            fs.read((char *)(item.bytes->items), size);
        }
    }
    item.startAddress = fs.readUInt64();
}

const char *__reverseFieldWrapper = "dleiFgnikcaB__k>";
string removeFieldWrapper(string name)
{
    auto walk = __reverseFieldWrapper;
    if (*name.begin() == '<' && name.size() >= 18)
    {
        auto iter = name.rbegin();
        while (*walk != '\x00')
        {
            if (*iter != *walk) {return name;}
            ++iter;
            ++walk;
        }
        
        auto offset = name.size() - 16;
        return string(name.begin() + 1, name.begin() + offset);
    }
    return name;
}

void readFieldDescription(FieldDescription &item, FileStream &fs)
{
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 4);
    
    item.name = removeFieldWrapper(fs.readString());
    item.offset = fs.readInt32();
    item.typeIndex = fs.readInt32();
    item.isStatic = fs.readBoolean();
}

void readTypeDescription(TypeDescription &item, FileStream &fs)
{
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 11);
    
    item.isValueType = fs.readBoolean();
    item.isArray = fs.readBoolean();
    item.arrayRank = fs.readInt32();
    item.name = fs.readString();
    item.assembly = fs.readString();
    {
        auto size = fs.readUInt32();
        item.fields = new Array<FieldDescription>(size);
        for (auto i = 0; i < size; i++)
        {
            readFieldDescription(item.fields->items[i], fs);
        }
    }
    {
        auto size = fs.readUInt32();
        if (size > 0)
        {
            item.staticFieldBytes = new Array<byte_t>(size);
            fs.read((char *)(item.staticFieldBytes->items), size);
        }
    }
    item.baseOrElementTypeIndex = fs.readInt32();
    item.size = fs.readInt32();
    item.typeInfoAddress = fs.readUInt64();
    item.typeIndex = fs.readInt32();
}

void readVirtualMachineInformation(VirtualMachineInformation &item, FileStream &fs)
{
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 7);
    
    item.pointerSize = fs.readInt32();
    item.objectHeaderSize = fs.readInt32();
    item.arrayHeaderSize = fs.readInt32();
    item.arrayBoundsOffsetInHeader = fs.readInt32();
    item.arraySizeOffsetInHeader = fs.readInt32();
    item.allocationGranularity = fs.readInt32();
    item.heapFormatVersion = fs.readInt32();
}

void MemorySnapshotReader::readPackedMemorySnapshot(PackedMemorySnapshot &item, FileStream &fs)
{
    __sampler.begin("ReadPackedMemorySnapshot");
    auto fieldCount = fs.readUInt8();
    assert(fieldCount == 7);
    
    {
        __sampler.begin("ReadNativeTypes");
        auto size = fs.readUInt32();
        item.nativeTypes = new Array<PackedNativeType>(size);
        for (auto i = 0; i < size; i++)
        {
            readPackedNativeType(item.nativeTypes->items[i], fs);
        }
        __sampler.end();
    }
    {
        __sampler.begin("ReadNativeObjects");
        auto size = fs.readUInt32();
        item.nativeObjects = new Array<PackedNativeUnityEngineObject>(size);
        for (auto i = 0; i < size; i++)
        {
            readPackedNativeUnityEngineObject(item.nativeObjects->items[i], fs);
        }
        __sampler.end();
    }
    {
        __sampler.begin("ReadGCHandles");
        auto size = fs.readUInt32();
        item.gcHandles = new Array<PackedGCHandle>(size);
        for (auto i = 0; i < size; i++)
        {
            readPackedGCHandle(item.gcHandles->items[i], fs);
        }
        __sampler.end();
    }
    {
        __sampler.begin("ReadConnections");
        auto size = fs.readUInt32();
        item.connections = new Array<Connection>(size);
        for (auto i = 0; i < size; i++)
        {
            readConnection(item.connections->items[i], fs);
        }
        __sampler.end();
    }
    {
        __sampler.begin("ReadHeapMemorySections");
        auto size = fs.readUInt32();
        item.heapSections = new Array<MemorySection>(size);
        for (auto i = 0; i < size; i++)
        {
            readMemorySection(item.heapSections->items[i], fs);
        }
        __sampler.end();
    }
    {
        __sampler.begin("ReadTypeDescriptions");
        auto size = fs.readUInt32();
        item.typeDescriptions = new Array<TypeDescription>(size);
        for (auto i = 0; i < size; i++)
        {
            readTypeDescription(item.typeDescriptions->items[i], fs);
        }
        __sampler.end();
    }
    __sampler.begin("ReadVirtualMatchineInformation");
    __sampler.begin("ReadObject");
    readVirtualMachineInformation(item.virtualMachineInformation, fs);
    __sampler.end(); // read_virtual_matchine_information
    __sampler.end(); // read_object
    
    __sampler.end(); // readPackedMemorySnapshot
}

void MemorySnapshotReader::readSnapshot(FileStream &fs)
{
    __vm = new VirtualMachineInformation;
    readVirtualMachineInformation(*__vm, fs);
    readPackedMemorySnapshot(*__snapshot, fs);
    
    prepareSnapshot();
}

bool strend(const string *s, const string *with)
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
    if (index == -1 && strend(&type.name, pattern))
    {
        index = type.typeIndex;
        return true;
    }
    
    return false;
}

void MemorySnapshotDeserializer::prepareSnapshot()
{
    __snapshot->uuid = uuid;
    
    __sampler.begin("PrepareSnapshot");
    __sampler.begin("CreateSortedHeapVector");
    for (auto i = 0; i < __snapshot->heapSections->size; i++)
    {
        MemorySection &heap = __snapshot->heapSections->items[i];
        heap.size = heap.bytes->size;
    }
    __sampler.end();
    
    __sampler.begin("CreateTypeStrings");
    
    // Premitive C# types
    string sSystemObject("System.Object");
    string sSystemString("System.String");
    string sSystemInt64("System.Int64");
    string sSystemInt32("System.Int32");
    string sSystemInt16("System.Int16");
    string sSystemSByte("System.SByte");
    string sSystemUInt64("System.UInt64");
    string sSystemUInt32("System.UInt32");
    string sSystemUInt16("System.UInt16");
    string sSystemByte("System.Byte");
    string sSystemChar("System.Char");
    string sSystemSingle("System.Single");
    string sSystemDouble("System.Double");
    string sSystemIntPtr("System.IntPtr");
    string sSystemBoolean("System.Boolean");
    string sSystemDelegate("System.Delegate");
    string sSystemMulticastDelegate("System.MulticastDelegate");
    string sSystemEnum("System.Enum");
    
    // Unity object types
    string sUnityEngineObject("UnityEngine.Object");
    string sSystemTextGenerator("UnityEngine.TextGenerator");
    string sCachedPtr("m_CachedPtr");
    __sampler.end();
    
    ManagedTypeIndex &managedTypeIndex = __snapshot->managedTypeIndex;
    
    __sampler.begin("ReadTypeIndex");
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
        else if (readTypeIndex(managedTypeIndex.unityengine_TextGenerator, type, &sSystemTextGenerator)) {}
        else if (readTypeIndex(managedTypeIndex.system_String, type, &sSystemString)) {}
        else if (readTypeIndex(managedTypeIndex.system_Object, type, &sSystemObject)) {}
        else if (readTypeIndex(managedTypeIndex.system_Int64, type, &sSystemInt64)) {}
        else if (readTypeIndex(managedTypeIndex.system_Int32, type, &sSystemInt32)) {}
        else if (readTypeIndex(managedTypeIndex.system_Int16, type, &sSystemInt16)) {}
        else if (readTypeIndex(managedTypeIndex.system_SByte, type, &sSystemSByte)) {}
        else if (readTypeIndex(managedTypeIndex.system_UInt64, type, &sSystemUInt64)) {}
        else if (readTypeIndex(managedTypeIndex.system_UInt32, type, &sSystemUInt32)) {}
        else if (readTypeIndex(managedTypeIndex.system_UInt16, type, &sSystemUInt16)) {}
        else if (readTypeIndex(managedTypeIndex.system_Byte, type, &sSystemByte)) {}
        else if (readTypeIndex(managedTypeIndex.system_Char, type, &sSystemChar)) {}
        else if (readTypeIndex(managedTypeIndex.system_Single, type, &sSystemSingle)) {}
        else if (readTypeIndex(managedTypeIndex.system_Double, type, &sSystemDouble)) {}
        else if (readTypeIndex(managedTypeIndex.system_IntPtr, type, &sSystemIntPtr)) {}
        else if (readTypeIndex(managedTypeIndex.system_Boolean, type, &sSystemBoolean)) {}
        else if (readTypeIndex(managedTypeIndex.system_Delegate, type, &sSystemDelegate)) {}
        else if (readTypeIndex(managedTypeIndex.system_MulticastDelegate, type, &sSystemMulticastDelegate)) {}
        else if (readTypeIndex(managedTypeIndex.system_Enum, type, &sSystemEnum)) {}
        
        if (type.fields != nullptr)
        {
            Array<FieldDescription> &fieldDescriptions = *type.fields;
            for (auto n = 0; n < fieldDescriptions.size; n++)
            {
                FieldDescription &field = fieldDescriptions[n];
                if (isUnityEngineObject && strend(&field.name, &sCachedPtr))
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
    
    __sampler.begin("SetNativeTypeIndex");
    string sMonoBehaviour("MonoBehaviour");
    string sFont("Font");
    string sGameObject("GameObject");
    string sTexture2D("Texture2D");
    string sSprite("Sprite");
    string sTransform("Transform");
    string sRectTransform("RectTransform");
    Array<PackedNativeType> &nativeTypes = *__snapshot->nativeTypes;
    for (auto i = 0; i < nativeTypes.size; i++)
    {
        auto &nt = nativeTypes[i];
        nt.typeIndex = i;
        if (strend(&nt.name, &sFont)) { __snapshot->nativeTypeIndex.Font = i; }
        else if (strcmp(nt.name.c_str(), sGameObject.c_str()) == 0) { __snapshot->nativeTypeIndex.GameObject = i; }
        else if (strcmp(nt.name.c_str(), sTexture2D.c_str()) == 0) { __snapshot->nativeTypeIndex.Texture2D = i; }
        else if (strcmp(nt.name.c_str(), sSprite.c_str()) == 0) { __snapshot->nativeTypeIndex.Sprite = i; }
        else if (strcmp(nt.name.c_str(), sTransform.c_str()) == 0) { __snapshot->nativeTypeIndex.Transform = i; }
        else if (strcmp(nt.name.c_str(), sRectTransform.c_str()) == 0) { __snapshot->nativeTypeIndex.RectTransform = i; }
        else if (strcmp(nt.name.c_str(), sMonoBehaviour.c_str()) == 0) { __snapshot->nativeTypeIndex.MonoBehaviour = i; }
    }
    __sampler.end();
    
    __sampler.begin("SetGCHandleIndex");
    Array<PackedGCHandle> &gcHandles = *__snapshot->gcHandles;
    for (auto i = 0; i < gcHandles.size; i++)
    {
        gcHandles[i].gcHandleArrayIndex = i;
    }
    __sampler.end();
    
    __sampler.begin("SetHeapIndex");
    Array<MemorySection> &managedHeapSections = *__snapshot->heapSections;
    
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
    
    __sampler.begin("SetNativeObjectIndex");
    Array<PackedNativeUnityEngineObject> &nativeObjects = *__snapshot->nativeObjects;
    for (auto i = 0; i < nativeObjects.size; i++)
    {
        nativeObjects[i].nativeObjectArrayIndex = i;
    }
    __sampler.end(); // set_native_object_index
    
    finishSnapshot();
    
    __sampler.end();
}

void MemorySnapshotDeserializer::finishSnapshot()
{
    __sampler.begin("FinishSnapshot");
    
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
