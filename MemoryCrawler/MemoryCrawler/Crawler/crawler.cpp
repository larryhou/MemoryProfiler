//
//  crawler.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/5.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "crawler.h"

void MemorySnapshotCrawler::crawl()
{
    __sampler.begin("MemorySnapshotCrawler");
    initManagedTypes();
    crawlGCHandles();
    crawlStatic();
    summarize();
    __sampler.end();
}

using std::ifstream;
void MemorySnapshotCrawler::debug()
{
#if USE_ADDRESS_MIRROR
    ifstream fs;
    fs.open("/Users/larryhou/Documents/MemoryProfiler/address.bin", ifstream::in | ifstream::binary);
    
    int32_t size = 0;
    char *ptr = (char *)&size;
    fs.read(ptr, 4);
    
    __mirror = new address_t[size];
    fs.read((char *)__mirror, size * sizeof(address_t));
    fs.close();
#endif
}

void MemorySnapshotCrawler::summarize()
{
    __sampler.begin("summarize_managed_objects");
    auto &nativeObjects = *snapshot.nativeObjects;
    auto &typeDescriptions = *snapshot.typeDescriptions;
    for (auto i = 0; i < typeDescriptions.size; i++)
    {
        auto &type = typeDescriptions[i];
        type.instanceMemory = 0;
        type.instanceCount = 0;
        type.nativeMemory = 0;
    }
    
    for (auto i = 0; i < managedObjects.size(); i++)
    {
        auto &mo = managedObjects[i];
        auto &type = typeDescriptions[mo.typeIndex];
        type.instanceMemory += mo.size;
        type.instanceCount += 1;
        
        if (mo.nativeObjectIndex >= 0)
        {
            auto &no = nativeObjects[mo.nativeObjectIndex];
            type.nativeMemory += no.size;
        }
    }
    __sampler.end();
}

void MemorySnapshotCrawler::initManagedTypes()
{
    __sampler.begin("initManagedTypes");
    Array<TypeDescription> &typeDescriptions = *snapshot.typeDescriptions;
    for (auto i = 0; i < typeDescriptions.size; i++)
    {
        TypeDescription &type = typeDescriptions[i];
        type.isUnityEngineObjectType = isSubclassOfManagedType(type, snapshot.managedTypeIndex.unityengine_Object);
    }
    __sampler.end();
}

const char16_t *MemorySnapshotCrawler::getString(address_t address, int32_t &size)
{
    return __memoryReader->readString(address + __vm->objectHeaderSize, size);
}

bool MemorySnapshotCrawler::isSubclassOfManagedType(TypeDescription &type, int32_t baseTypeIndex)
{
    if (type.typeIndex == baseTypeIndex) { return true; }
    if (type.typeIndex < 0 || baseTypeIndex < 0) { return false; }
    
    TypeDescription *iter = &type;
    while (iter->baseOrElementTypeIndex != -1)
    {
        iter = &snapshot.typeDescriptions->items[iter->baseOrElementTypeIndex];
        if (iter->typeIndex == baseTypeIndex) { return true; }
    }
    
    return false;
}

bool MemorySnapshotCrawler::isSubclassOfNativeType(PackedNativeType &type, int32_t baseTypeIndex)
{
    if (type.typeIndex == baseTypeIndex) { return true; }
    if (type.typeIndex < 0 || baseTypeIndex < 0) { return false; }
    
    PackedNativeType *iter = &type;
    while (iter->nativeBaseTypeArrayIndex != -1)
    {
        iter = &snapshot.nativeTypes->items[iter->nativeBaseTypeArrayIndex];
        if (iter->typeIndex == baseTypeIndex) { return true; }
    }
    
    return false;
}

void MemorySnapshotCrawler::tryAcceptConnection(EntityConnection &ec)
{
    if (ec.fromKind != ConnectionKind::None && ec.from >= 0)
    {
        auto &mo = managedObjects[ec.from];
        if (mo.toConnections == nullptr)
        {
            mo.toConnections = new vector<int32_t>;
        }
        mo.toConnections->push_back(ec.connectionArrayIndex);
    }
    
    if (ec.toKind != ConnectionKind::None && ec.to >= 0)
    {
        auto &mo = managedObjects[ec.to];
        if (mo.fromConnections == nullptr)
        {
            mo.fromConnections = new vector<int32_t>;
        }
        mo.fromConnections->push_back(ec.connectionArrayIndex);
    }
}

int32_t MemorySnapshotCrawler::findTypeAtTypeInfoAddress(address_t address)
{
    if (address == 0) {return -1;}
    if (__typeAddressMap.size() == 0)
    {
        Array<TypeDescription> &typeDescriptions = *snapshot.typeDescriptions;
        for (auto i = 0; i < typeDescriptions.size; i++)
        {
            auto &type = typeDescriptions[i];
            __typeAddressMap.insert(pair<address_t, int32_t>(type.typeInfoAddress, type.typeIndex));
        }
    }
    
    auto iter = __typeAddressMap.find(address);
    return iter != __typeAddressMap.end() ? iter->second : -1;
}

int32_t MemorySnapshotCrawler::findTypeOfAddress(address_t address)
{
    auto typeIndex = findTypeAtTypeInfoAddress(address);
    if (typeIndex != -1) {return typeIndex;}
    auto typePtr = __memoryReader->readPointer(address);
    if (typePtr == 0) {return -1;}
    auto vtablePtr = __memoryReader->readPointer(typePtr);
    if (vtablePtr != 0)
    {
        return findTypeAtTypeInfoAddress(vtablePtr);
    }
    return findTypeAtTypeInfoAddress(typePtr);
}

int32_t MemorySnapshotCrawler::findManagedObjectOfNativeObject(address_t address)
{
    if (address == 0){return -1;}
    if (__managedNativeAddressMap.size() == 0)
    {
        for (auto i = 0; i < managedObjects.size(); i++)
        {
            auto &mo = managedObjects[i];
            if (mo.nativeObjectIndex >= 0)
            {
                auto &no = snapshot.nativeObjects->items[mo.nativeObjectIndex];
                __managedNativeAddressMap.insert(pair<address_t, int32_t>(no.nativeObjectAddress, mo.managedObjectIndex));
            }
        }
    }
    
    auto iter = __managedNativeAddressMap.find(address);
    return iter != __managedNativeAddressMap.end() ? iter->second : -1;
}

int32_t MemorySnapshotCrawler::findManagedObjectAtAddress(address_t address)
{
    if (address == 0){return -1;}
    if (__managedObjectAddressMap.size() == 0)
    {
        for (auto i = 0; i < managedObjects.size(); i++)
        {
            auto &mo = managedObjects[i];
            __managedObjectAddressMap.insert(pair<address_t, int32_t>(mo.address, mo.managedObjectIndex));
        }
    }
    
    auto iter = __managedObjectAddressMap.find(address);
    return iter != __managedObjectAddressMap.end() ? iter->second : -1;
}

int32_t MemorySnapshotCrawler::findNativeObjectAtAddress(address_t address)
{
    if (address == 0){return -1;}
    if (__nativeObjectAddressMap.size() == 0)
    {
        auto &nativeObjects = *snapshot.nativeObjects;
        for (auto i = 0; i < nativeObjects.size; i++)
        {
            auto &no = nativeObjects[i];
            __nativeObjectAddressMap.insert(pair<address_t, int32_t>(no.nativeObjectAddress, no.nativeObjectArrayIndex));
        }
    }
    
    auto iter = __nativeObjectAddressMap.find(address);
    return iter != __nativeObjectAddressMap.end() ? iter->second : -1;
}

int32_t MemorySnapshotCrawler::findGCHandleWithTargetAddress(address_t address)
{
    if (address == 0){return -1;}
    if (__gcHandleAddressMap.size() == 0)
    {
        auto &gcHandles = *snapshot.gcHandles;
        for (auto i = 0; i < gcHandles.size; i++)
        {
            auto &no = gcHandles[i];
            __gcHandleAddressMap.insert(pair<address_t, int32_t>(no.target, no.gcHandleArrayIndex));
        }
    }
    
    auto iter = __gcHandleAddressMap.find(address);
    return iter != __gcHandleAddressMap.end() ? iter->second : -1;
}

void MemorySnapshotCrawler::tryConnectWithNativeObject(ManagedObject &mo)
{
    if (mo.nativeObjectIndex >= 0){return;}
    
    auto &type = snapshot.typeDescriptions->items[mo.typeIndex];
    mo.isValueType = type.isValueType;
    if (mo.isValueType || type.isArray || !type.isUnityEngineObjectType) {return;}
    
    auto nativeAddress = __memoryReader->readPointer(mo.address + snapshot.cached_ptr->offset);
    if (nativeAddress == 0){return;}
    
    auto nativeObjectIndex = findNativeObjectAtAddress(nativeAddress);
    if (nativeObjectIndex == -1){return;}
    
    // connect managed/native objects
    auto &no = snapshot.nativeObjects->items[nativeObjectIndex];
    mo.nativeObjectIndex = nativeObjectIndex;
    mo.nativeSize = no.size;
    no.managedObjectArrayIndex = mo.managedObjectIndex;
    
    // connect managed/native types
    auto &nativeType = snapshot.nativeTypes->items[no.nativeTypeArrayIndex];
    type.nativeTypeArrayIndex = nativeType.typeIndex;
    nativeType.managedTypeArrayIndex = type.typeIndex;
}

inline void MemorySnapshotCrawler::setObjectSize(ManagedObject &mo, TypeDescription &type, HeapMemoryReader &memoryReader)
{
    if (mo.size != 0){return;}
    mo.size = memoryReader.readObjectSize(mo.address, type);
}

ManagedObject &MemorySnapshotCrawler::createManagedObject(address_t address, int32_t typeIndex)
{
    auto &mo = managedObjects.add();
    mo.address = address;
    mo.typeIndex = typeIndex;
    mo.managedObjectIndex = managedObjects.size() - 1;
    tryConnectWithNativeObject(mo);
#if USE_ADDRESS_MIRROR
    assert(address == __mirror[mo.managedObjectIndex]);
#endif
    return mo;
}

bool MemorySnapshotCrawler::isCrawlable(TypeDescription &type)
{
    return !type.isValueType || type.size > 8; // vm->pointerSize
}

bool MemorySnapshotCrawler::crawlManagedArrayAddress(address_t address, TypeDescription &type, HeapMemoryReader &memoryReader, EntityJoint &joint, int32_t depth)
{
    auto isStaticCrawling = memoryReader.isStatic();
    if (address < 0 || (!isStaticCrawling && address == 0)){return false;}
    
    auto &elementType = snapshot.typeDescriptions->items[type.baseOrElementTypeIndex];
    if (!isCrawlable(elementType)) {return false;}
    
    auto successCount = 0;
    address_t elementAddress = 0;
    auto elementCount = memoryReader.readArrayLength(address, type);
    for (auto i = 0; i < elementCount; i++)
    {
        if (elementType.isValueType)
        {
            elementAddress = address + __vm->arrayHeaderSize + i *elementType.size - __vm->objectHeaderSize;
        }
        else
        {
            auto ptrAddress = address + __vm->arrayHeaderSize + i * __vm->pointerSize;
            elementAddress = memoryReader.readPointer(ptrAddress);
        }
        
        auto &elementJoint = joints.clone(joint);
        elementJoint.jointArrayIndex = joints.size() - 1;
        elementJoint.isConnected = false;
        
        // set element info
        elementJoint.elementArrayIndex = i;
        
        auto success = crawlManagedEntryAddress(elementAddress, &elementType, memoryReader, elementJoint, false, depth + 1);
        if (success) { successCount += 1; }
    }
    
    return successCount != 0;
}

bool MemorySnapshotCrawler::crawlManagedEntryAddress(address_t address, TypeDescription *type, HeapMemoryReader &memoryReader, EntityJoint &joint, bool isActualType, int32_t depth)
{
    auto isStaticCrawling = memoryReader.isStatic();
    if (address < 0 || (!isStaticCrawling && address == 0)){return false;}
    if (depth >= 512) {return false;}
    
    int32_t typeIndex = -1;
    if (type != nullptr && type->isValueType)
    {
        typeIndex = type->typeIndex;
    }
    else
    {
        if (isActualType)
        {
            typeIndex = type->typeIndex;
        }
        else
        {
            typeIndex = findTypeOfAddress(address);
            if (typeIndex == -1 && type != nullptr) {typeIndex = type->typeIndex;}
        }
    }
    if (typeIndex == -1){return false;}
    
    auto &entryType = snapshot.typeDescriptions->items[typeIndex];
    
    ManagedObject *mo;
    auto iter = __crawlingVisit.find(address);
    if (entryType.isValueType || iter == __crawlingVisit.end())
    {
        mo = &createManagedObject(address, typeIndex);
        mo->jointArrayIndex = joint.jointArrayIndex;
        mo->gcHandleIndex = joint.gcHandleIndex;
        setObjectSize(*mo, entryType, memoryReader);
        joint.isConnected = true;
    }
    else
    {
        auto managedObjectIndex = iter->second;
        mo = &managedObjects[managedObjectIndex];
    }
    
    assert(mo->managedObjectIndex >= 0);
    
    auto &ec = connections.add();
    ec.connectionArrayIndex = connections.size() - 1;
    if (joint.gcHandleIndex >= 0)
    {
        ec.fromKind = ConnectionKind::gcHandle;
        ec.from = -1;
    }
    else if (joint.isStatic)
    {
        ec.fromKind = ConnectionKind::Static;
        ec.from = -1;
    }
    else
    {
        ec.fromKind = ConnectionKind::Managed;
        ec.from = joint.hookObjectIndex;
    }
    
    ec.toKind = ConnectionKind::Managed;
    ec.to = mo->managedObjectIndex;
    tryAcceptConnection(ec);
    
    if (!entryType.isValueType)
    {
        if (iter != __crawlingVisit.end()) {return false;}
        __crawlingVisit.insert(pair<address_t, int32_t>(address, mo->managedObjectIndex));
    }
    
    if (entryType.isArray)
    {
        return crawlManagedArrayAddress(address, entryType, memoryReader, joint, depth + 1);
    }
    
    auto successCount = 0;
    auto iterType = &entryType;
    while (iterType != nullptr)
    {
        for (auto i = 0; i < iterType->fields->size; i++)
        {
            auto &field = iterType->fields->items[i];
            if (field.isStatic){continue;}
            
            auto *fieldType = &snapshot.typeDescriptions->items[field.typeIndex];
            if (!isCrawlable(*fieldType)){continue;}
            
            address_t fieldAddress = 0;
            if (fieldType->isValueType)
            {
                fieldAddress = address + field.offset - __vm->objectHeaderSize;
            }
            else
            {
                address_t ptrAddress = 0;
                if (memoryReader.isStatic())
                {
                    ptrAddress = address + field.offset - __vm->objectHeaderSize;
                }
                else
                {
                    ptrAddress = address + field.offset;
                }
                fieldAddress = memoryReader.readPointer(ptrAddress);
                auto fieldTypeIndex = findTypeOfAddress(fieldAddress);
                if (fieldTypeIndex != -1)
                {
                    fieldType = &snapshot.typeDescriptions->items[fieldTypeIndex];
                }
            }
            
            auto *reader = &memoryReader;
            if (!fieldType->isValueType)
            {
                reader = this->__memoryReader;
            }
            
            auto &ej = joints.add();
            
            // set field hook info
            ej.jointArrayIndex = joints.size() - 1;
            ej.hookObjectAddress = address;
            ej.hookObjectIndex = mo->managedObjectIndex;
            ej.hookTypeIndex = entryType.typeIndex;
            
            // set field info
            ej.fieldAddress = fieldAddress;
            ej.fieldTypeIndex = field.typeIndex;
            ej.fieldSlotIndex = field.fieldSlotIndex;
            ej.fieldOffset = field.offset;
            
            auto success = crawlManagedEntryAddress(fieldAddress, fieldType, *reader, ej, true, depth + 1);
            if (success) { successCount += 1; }
        }
        
        if (iterType->baseOrElementTypeIndex == -1)
        {
            iterType = nullptr;
        }
        else
        {
            iterType = &snapshot.typeDescriptions->items[iterType->baseOrElementTypeIndex];
        }
    }
    
    return successCount != 0;
}

void MemorySnapshotCrawler::crawlGCHandles()
{
    __sampler.begin("crawlGCHandles");
    auto &gcHandles = *snapshot.gcHandles;
    for (auto i = 0; i < gcHandles.size; i++)
    {
        auto &item = gcHandles[i];
        
        auto &joint = joints.add();
        joint.jointArrayIndex = joints.size() - 1;
        
        // set gcHandle info
        joint.gcHandleIndex = item.gcHandleArrayIndex;
        
        crawlManagedEntryAddress(item.target, nullptr, *__memoryReader, joint, false, 0);
    }
    __sampler.end();
}

void MemorySnapshotCrawler::crawlStatic()
{
    __sampler.begin("crawlStatic");
    auto &typeDescriptions = *snapshot.typeDescriptions;
    for (auto i = 0; i < typeDescriptions.size; i++)
    {
        auto &type = typeDescriptions[i];
        if (type.staticFieldBytes == nullptr || type.staticFieldBytes->size == 0){continue;}
        for (auto n = 0; n < type.fields->size; n++)
        {
            auto &field = type.fields->items[n];
            if (!field.isStatic){continue;}
            
            __staticMemoryReader->load(*type.staticFieldBytes);
            
            HeapMemoryReader *reader;
            address_t fieldAddress = 0;
            auto *fieldType = &snapshot.typeDescriptions->items[field.typeIndex];
            if (fieldType->isValueType)
            {
                fieldAddress = field.offset - __vm->objectHeaderSize;
                reader = __staticMemoryReader;
            }
            else
            {
                fieldAddress = __staticMemoryReader->readPointer(field.offset);
                reader = __memoryReader;
            }
            
            auto &joint = joints.add();
            joint.jointArrayIndex = joints.size() - 1;
            
            // set static field info
            joint.hookTypeIndex = type.typeIndex;
            joint.fieldSlotIndex = field.fieldSlotIndex;
            joint.fieldTypeIndex = field.typeIndex;
            joint.isStatic = true;
            
            crawlManagedEntryAddress(fieldAddress, fieldType, *reader, joint, false, 0);
        }
    }
    __sampler.end();
}

MemorySnapshotCrawler::~MemorySnapshotCrawler()
{
    __sampler.summary();
    
    delete __mirror;
    delete __memoryReader;
    delete __staticMemoryReader;
}
