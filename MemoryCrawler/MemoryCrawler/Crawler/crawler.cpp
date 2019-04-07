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
    initManagedTypes();
    crawlGCHandles();
    crawlStatic();
}

void MemorySnapshotCrawler::initManagedTypes()
{
    Array<TypeDescription> &typeDescriptions = *__snapshot.typeDescriptions;
    for (auto i = 0; i < typeDescriptions.size; i++)
    {
        TypeDescription &type = typeDescriptions[i];
        type.isUnityEngineObjectType = isSubclassOfManagedType(type, __snapshot.managedTypeIndex.unityengine_Object);
    }
}

bool MemorySnapshotCrawler::isSubclassOfManagedType(TypeDescription &type, int32_t baseTypeIndex)
{
    if (type.typeIndex == baseTypeIndex) { return true; }
    if (type.typeIndex < 0 || baseTypeIndex < 0) { return false; }
    
    TypeDescription *iter = &type;
    while (iter->baseOrElementTypeIndex != -1)
    {
        iter = &__snapshot.typeDescriptions->items[iter->baseOrElementTypeIndex];
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
        iter = &__snapshot.nativeTypes->items[iter->nativeBaseTypeArrayIndex];
        if (iter->typeIndex == baseTypeIndex) { return true; }
    }
    
    return false;
}

void MemorySnapshotCrawler::tryAcceptConnection(JointConnection &jc)
{
    if (jc.from >= 0)
    {
        ManagedObject &fromObject = managedObjects[jc.from];
        auto iter = __connectionVisit.find(fromObject.address);
        if (iter != __connectionVisit.end())
        {
            ManagedObject &toObject = managedObjects[jc.to];
            if (iter->second == toObject.address) { return; }
            __connectionVisit.insert(pair<address_t, address_t>(fromObject.address, toObject.address));
        }
    }
    
    if (jc.fromKind != ConnectionKind::None && jc.from >= 0)
    {
        auto key = getIndexKey(jc.fromKind, jc.from);
        auto iter = fromConnections.find(key);
        if (iter == fromConnections.end())
        {
            auto entity = fromConnections.insert(pair<int32_t, vector<int32_t> *>(key, new vector<int32_t>));
            iter = entity.first;
        }
        iter->second->push_back(jc.connectionArrayIndex);
    }
    
    if (jc.toKind != ConnectionKind::None && jc.to >= 0)
    {
        auto key = getIndexKey(jc.toKind, jc.to);
        auto iter = toConnections.find(key);
        if (iter == toConnections.end())
        {
            auto entity = toConnections.insert(pair<int32_t, vector<int32_t> *>(key, new vector<int32_t>));
            iter = entity.first;
        }
        iter->second->push_back(jc.connectionArrayIndex);
    }
}

int32_t MemorySnapshotCrawler::getIndexKey(ConnectionKind kind, int32_t index)
{
    return ((int32_t)kind << 28) + index;
}

int64_t MemorySnapshotCrawler::getConnectionKey(JointConnection &jc)
{
    return (int64_t)getIndexKey(jc.fromKind, jc.from) << 32 | getIndexKey(jc.toKind, jc.to);
}

int32_t MemorySnapshotCrawler::findTypeAtTypeInfoAddress(address_t address)
{
    if (address == 0) {return -1;}
    if (__typeAddressMap.size() == 0)
    {
        Array<TypeDescription> &typeDescriptions = *__snapshot.typeDescriptions;
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
                auto &no = __snapshot.nativeObjects->items[mo.nativeObjectIndex];
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
        auto &nativeObjects = *__snapshot.nativeObjects;
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
        auto &gcHandles = *__snapshot.gcHandles;
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
    
    auto &type = __snapshot.typeDescriptions->items[mo.typeIndex];
    mo.isValueType = type.isValueType;
    if (mo.isValueType || type.isArray || !type.isUnityEngineObjectType) {return;}
    
    auto nativeAddress = __memoryReader->readPointer(mo.address + __snapshot.cached_ptr->offset);
    if (nativeAddress == 0){return;}
    
    auto nativeObjectIndex = findNativeObjectAtAddress(nativeAddress);
    if (nativeObjectIndex == -1){return;}
    
    // connect managed/native objects
    auto &no = __snapshot.nativeObjects->items[nativeObjectIndex];
    mo.nativeObjectIndex = nativeObjectIndex;
    mo.nativeSize = no.size;
    no.managedObjectArrayIndex = mo.managedObjectIndex;
    
    // connect managed/native types
    auto &nativeType = __snapshot.nativeTypes->items[no.nativeTypeArrayIndex];
    type.nativeTypeArrayIndex = nativeType.typeIndex;
    nativeType.managedTypeArrayIndex = type.typeIndex;
}

void MemorySnapshotCrawler::setObjectSize(ManagedObject &mo, TypeDescription &type, HeapMemoryReader &memoryReader)
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
    return mo;
}

bool MemorySnapshotCrawler::isCrawlable(TypeDescription &type)
{
    return !type.isValueType || type.size > 8; // vm->pointerSize
}

void MemorySnapshotCrawler::crawlManagedArrayAddress(address_t address, TypeDescription &type, HeapMemoryReader &memoryReader, MemberJoint &joint, int32_t depth)
{
    auto isStaticCrawling = memoryReader.isStatic();
    if (address < 0 || (!isStaticCrawling && address == 0)){return;}
    
    auto &elementType = __snapshot.typeDescriptions->items[type.baseOrElementTypeIndex];
    if (!isCrawlable(elementType)) {return;}
    
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
        
        // set element info
        elementJoint.arrayIndex = i;
        
        crawlManagedEntryAddress(elementAddress, &elementType, memoryReader, elementJoint, false, depth + 1);
    }
}

void MemorySnapshotCrawler::crawlManagedEntryAddress(address_t address, TypeDescription *type, HeapMemoryReader &memoryReader, MemberJoint &joint, bool isRealType, int32_t depth)
{
    auto isStaticCrawling = memoryReader.isStatic();
    if (address < 0 || (!isStaticCrawling && address == 0)){return;}
    if (depth >= 512) {return;}
    
    int32_t typeIndex = -1;
    if (type != nullptr && type->isValueType)
    {
        typeIndex = type->typeIndex;
    }
    else
    {
        if (isRealType)
        {
            typeIndex = type->typeIndex;
        }
        else
        {
            typeIndex = findTypeOfAddress(address);
            if (typeIndex == -1 && type != nullptr) {typeIndex = type->typeIndex;}
        }
    }
    if (typeIndex == -1){return;}
    
    auto &entryType = __snapshot.typeDescriptions->items[typeIndex];
    
    ManagedObject *mo;
    auto iter = __visit.find(address);
    if (entryType.isValueType || iter == __visit.end())
    {
        mo = &createManagedObject(address, typeIndex);
        mo->jointArrayIndex = joint.jointArrayIndex;
        mo->gcHandleIndex = joint.gcHandleIndex;
        setObjectSize(*mo, entryType, memoryReader);
    }
    else
    {
        auto managedObjectIndex = iter->second;
        mo = &managedObjects[managedObjectIndex];
    }
    
    assert(mo->managedObjectIndex >= 0);
    
    auto &jc = connections.add();
    jc.connectionArrayIndex = connections.size() - 1;
    if (joint.gcHandleIndex >= 0)
    {
        jc.fromKind = ConnectionKind::gcHandle;
        jc.from = -1;
    }
    else if (joint.isStatic)
    {
        jc.fromKind = ConnectionKind::Static;
        jc.from = -1;
    }
    else
    {
        jc.fromKind = ConnectionKind::Managed;
        jc.from = joint.hookObjectIndex;
    }
    
    jc.jointArrayIndex = joint.jointArrayIndex;
    jc.toKind = ConnectionKind::Managed;
    jc.to = mo->managedObjectIndex;
    tryAcceptConnection(jc);
    
    if (!entryType.isValueType)
    {
        if (iter != __visit.end()) {return;}
        __visit.insert(pair<address_t, int32_t>(address, mo->managedObjectIndex));
    }
    
    if (entryType.isArray)
    {
        crawlManagedArrayAddress(address, entryType, memoryReader, joint, depth + 1);
        return;
    }
    
    auto iterType = &entryType;
    while (iterType != nullptr)
    {
        for (auto i = 0; iterType->fields->size; i++)
        {
            auto &field = iterType->fields->items[i];
            if (field.isStatic){continue;}
            
            auto *fieldType = &__snapshot.typeDescriptions->items[field.typeIndex];
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
                if (fieldTypeIndex == -1)
                {
                    fieldType = &__snapshot.typeDescriptions->items[fieldTypeIndex];
                }
            }
            
            auto *reader = &memoryReader;
            if (!fieldType->isValueType)
            {
                reader = this->__memoryReader;
            }
            
            auto &fj = joints.add();
            
            // set field hook info
            fj.jointArrayIndex = joints.size() - 1;
            fj.hookObjectAddress = address;
            fj.hookObjectIndex = mo->managedObjectIndex;
            fj.hookTypeIndex = entryType.typeIndex;
            
            // set field info
            fj.fieldTypeIndex = field.typeIndex;
            fj.fieldSlotIndex = field.fieldSlotIndex;
            fj.fieldOffset = field.offset;
            fj.fieldAddress = fieldAddress;
            
            crawlManagedEntryAddress(fieldAddress, fieldType, *reader, fj, true, depth + 1);
        }
        
        if (iterType->baseOrElementTypeIndex == -1)
        {
            iterType = nullptr;
        }
        else
        {
            iterType = &__snapshot.typeDescriptions->items[iterType->baseOrElementTypeIndex];
        }
    }
}

void MemorySnapshotCrawler::crawlGCHandles()
{
    auto &gcHandles = *__snapshot.gcHandles;
    for (auto i = 0; i < gcHandles.size; i++)
    {
        auto &item = gcHandles[i];
        
        auto &joint = joints.add();
        joint.jointArrayIndex = joints.size() - 1;
        
        // set gcHandle info
        joint.gcHandleIndex = item.gcHandleArrayIndex;
        
        crawlManagedEntryAddress(item.target, nullptr, *__memoryReader, joint, false, 0);
    }
}

void MemorySnapshotCrawler::crawlStatic()
{
    StaticMemoryReader staticMemoryReader(__snapshot);
    auto &typeDescriptions = *__snapshot.typeDescriptions;
    for (auto i = 0; i < typeDescriptions.size; i++)
    {
        auto &type = typeDescriptions[i];
        if (type.staticFieldBytes == nullptr || type.staticFieldBytes->size == 0){continue;}
        for (auto n = 0; n < type.fields->size; n++)
        {
            auto &field = type.fields->items[n];
            if (!field.isStatic){continue;}
            
            staticMemoryReader.load(*type.staticFieldBytes);
            
            HeapMemoryReader *reader;
            address_t fieldAddress = 0;
            auto *fieldType = &__snapshot.typeDescriptions->items[field.typeIndex];
            if (fieldType->isValueType)
            {
                fieldAddress = field.offset - __vm->objectHeaderSize;
                reader = &staticMemoryReader;
            }
            else
            {
                fieldAddress = staticMemoryReader.readPointer(field.offset);
                reader = this->__memoryReader;
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
    
}
