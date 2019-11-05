//
//  format.cpp
//  MemoryCrawler
//
//  Created by LARRYHOU on 2019/11/5.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "format.h"

void HeapExplorerFormat::encodeHeader()
{
    __fs.write('h');
    __fs.write('e');
    __fs.write('a');
    __fs.write('p');
    
    __fs.write<int32_t>(2);
    __fs.writeBWString("1.0");
    __fs.writeBWString("MemoryCrawler");
    __fs.writeBWString("https://github.com/larryhou/MemoryProfiler");
    __fs.write<bool>(false);
}

void HeapExplorerFormat::encode(Array<PackedNativeType> &nativeTypes)
{
    __fs.write<int32_t>(1);
    __fs.write<int32_t>(nativeTypes.size);
    
    auto iter = nativeTypes.items;
    for (auto i = 0; i < nativeTypes.size; i++)
    {
        __fs.writeBWString(iter->name.c_str());
        __fs.write<int32_t>(iter->nativeBaseTypeArrayIndex);
        ++iter;
    }
}

void HeapExplorerFormat::encode(Array<PackedNativeUnityEngineObject> &nativeObjects)
{
    __fs.write<int32_t>(1);
    __fs.write<int32_t>(nativeObjects.size);
    
    auto iter = nativeObjects.items;
    for (auto i = 0; i < nativeObjects.size; i++)
    {
        __fs.write<bool>(iter->isPersistent);
        __fs.write<bool>(iter->isDontDestroyOnLoad);
        __fs.write<bool>(iter->isManager);
        __fs.writeBWString(iter->name.c_str());
        __fs.write<int32_t>(iter->instanceId);
        __fs.write<int32_t>(iter->size);
        __fs.write<int32_t>(iter->nativeTypeArrayIndex);
        __fs.write<int32_t>(iter->hideFlags);
        __fs.write<uint64_t>(iter->nativeObjectAddress);
        ++iter;
    }
}

void HeapExplorerFormat::encode(Array<PackedGCHandle> &gcHandles)
{
    __fs.write<int32_t>(1);
    __fs.write<int32_t>(gcHandles.size);
    
    auto iter = gcHandles.items;
    for (auto i = 0; i < gcHandles.size; i++)
    {
        __fs.write<uint64_t>(iter->target);
        ++iter;
    }
}

void HeapExplorerFormat::encode(Array<Connection> &connections)
{
    __fs.write<int32_t>(1);
    __fs.write<int32_t>(connections.size);
    
    auto iter = connections.items;
    for (auto i = 0; i < connections.size; i++)
    {
        __fs.write<int32_t>(iter->from);
        __fs.write<int32_t>(iter->to);
        ++iter;
    }
}

void HeapExplorerFormat::encode(Array<MemorySection> &heapSections)
{
    __fs.write<int32_t>(1);
    __fs.write<int32_t>(heapSections.size);
    
    auto iter = heapSections.items;
    for (auto i = 0; i < heapSections.size; i++)
    {
        __fs.write<int32_t>(iter->bytes->size);
        __fs.write((const char *)iter->bytes->items, iter->bytes->size);
        __fs.write<uint64_t>(iter->startAddress);
        ++iter;
    }
}

void HeapExplorerFormat::encode(Array<TypeDescription> &typeDescriptions)
{
    __fs.write<int32_t>(1);
    __fs.write<int32_t>(typeDescriptions.size);
    
    auto iter = typeDescriptions.items;
    for (auto i = 0; i < typeDescriptions.size; i++)
    {
        __fs.write<bool>(iter->isValueType);
        __fs.write<bool>(iter->isArray);
        __fs.write<int32_t>(iter->arrayRank);
        __fs.writeBWString(iter->name.c_str());
        __fs.writeBWString(iter->assembly.c_str());
        if (iter->staticFieldBytes != nullptr)
        {
            __fs.write<int32_t>(iter->staticFieldBytes->size);
            __fs.write((const char*)iter->staticFieldBytes->items, iter->staticFieldBytes->size);
        }
        else
        {
            __fs.write<int32_t>(0);
        }
        
        __fs.write<int32_t>(iter->baseOrElementTypeIndex);
        __fs.write<int32_t>(iter->size);
        __fs.write<uint64_t>(iter->typeInfoAddress);
        __fs.write<int32_t>(iter->typeIndex);
        
        // fields
        __fs.write<int32_t>(1);
        if (!iter->isArray)
        {
            auto &fields = *iter->fields;
            __fs.write<int32_t>(fields.size);
            for (auto n = 0; n < fields.size; n++)
            {
                auto &item = fields.items[n];
                __fs.writeBWString(item.name.c_str());
                __fs.write<int32_t>(item.offset);
                __fs.write<int32_t>(item.typeIndex);
                __fs.write<bool>(item.isStatic);
            }
        } else { __fs.write<int32_t>(0); }
        
        ++iter;
    }
}

void HeapExplorerFormat::encode(VirtualMachineInformation &vm)
{
    __fs.write<int32_t>(1);
    __fs.write<int32_t>(vm.pointerSize);
    __fs.write<int32_t>(vm.objectHeaderSize);
    __fs.write<int32_t>(vm.arrayHeaderSize);
    __fs.write<int32_t>(vm.arrayBoundsOffsetInHeader);
    __fs.write<int32_t>(vm.arraySizeOffsetInHeader);
    __fs.write<int32_t>(vm.allocationGranularity);
    __fs.write<int32_t>(vm.heapFormatVersion);
}
