//
//  heap.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/6.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "heap.h"

int32_t HeapMemoryReader::seekOffset(address_t address)
{
    if (address == 0) {return -1;}
    if (address >= startAddress && address < stopAddress)
    {
        return (int32_t)(address - startAddress);
    }
    
    auto heapIndex = findHeapOfAddress(address);
    if (heapIndex == -1) {return -1;}
    
    MemorySection &heap = managedHeapSections->items[heapIndex];
    memory = heap.bytes->items;
    startAddress = heap.startAddress;
    stopAddress = heap.startAddress + heap.bytes->size;
    size = heap.bytes->size;
    
    return (int32_t)(address - startAddress);
}

uint32_t HeapMemoryReader::readStringLength(address_t address)
{
    auto offset = seekOffset(address);
    if (offset == -1) {return 0;}
    
    auto length = readInt32(address);
    return length > 0 ? length : 0;
}

int32_t HeapMemoryReader::readString(address_t address, char16_t *buffer)
{
    auto offset = seekOffset(address);
    if (offset == -1) {return -1;}
    
    auto length = readInt32(address);
    if (length <= 0) {return -2;}
    
    offset += 4;
    memcpy(buffer, memory + offset, length << 1);
    return length;
}

uint32_t HeapMemoryReader::readArrayLength(address_t address, TypeDescription &type)
{
    auto offset = seekOffset(address);
    if (offset == -1) {return 0;}
    
    auto bounds = readPointer(address + vm->arrayBoundsOffsetInHeader);
    if (bounds == 0)
    {
        return (uint32_t)readPointer(address + vm->arraySizeOffsetInHeader);
    }
    
    auto length = (uint64_t)1;
    auto cursor = bounds;
    for (auto i = 0; i < type.arrayRank; i++)
    {
        length *= readPointer(cursor);
        cursor += vm->pointerSize;
    }
    return (uint32_t)length;
}

static string sTypeString("System.String");
uint32_t HeapMemoryReader::readObjectSize(address_t address, TypeDescription &type)
{
    auto offset = seekOffset(address);
    if (offset == -1) {return 0;}
    if (type.isArray)
    {
        if (type.baseOrElementTypeIndex < 0 || type.baseOrElementTypeIndex >= snapshot.typeDescriptions->size)
        {
            return 0;
        }
        auto elementCount = readArrayLength(address, type);
        auto elementType = snapshot.typeDescriptions->items[type.baseOrElementTypeIndex];
        auto elementSize = elementType.isValueType ? elementType.size : vm->pointerSize;
        return vm->arrayHeaderSize + elementSize * elementCount;
    }
    
    if (sTypeString.compare(*type.name) == 0)
    {
        auto size = vm->objectHeaderSize;
        size += 4; // string length
        size += readStringLength(address + vm->objectHeaderSize) * 2; // char16_t
        size += 2; // \x00\x00
        return size;
    }
    
    return type.size;
}

HeapSegment HeapMemoryReader::readObjectMemory(address_t address, TypeDescription &type)
{
    HeapSegment segment;
    auto size = readObjectSize(address, type);
    if (size <= 0) { return segment; }
    
    auto offset = seekOffset(address);
    if (offset < 0) { return segment; }
    
    segment.begin = (char *)memory + offset;
    segment.end = segment.begin + size;
    return segment;
}

int32_t HeapMemoryReader::findHeapOfAddress(address_t address)
{
    int32_t min = 0, max = managedHeapSections->size - 1;
    Array<MemorySection> &heapSections = *managedHeapSections;
    
    while (min <= max)
    {
        auto mid = (min + max) >> 1;
        MemorySection &heap = heapSections[mid];
        if (heap.startAddress > address)
        {
            max = mid - 1;
        }
        else if (heap.startAddress + heap.bytes->size < address)
        {
            min = mid + 1;
        }
        else
        {
            return mid;
        }
    }
    
    return -1;
}

void StaticMemoryReader::load(const byte_t *bytes, int32_t size)
{
    memory = bytes;
    startAddress = 0;
    stopAddress = size;
    this->size = size;
}

int32_t StaticMemoryReader::seekOffset(address_t address)
{
    if (address < 0 || size == 0 || address >= size) {return -1;}
    return (int32_t)address;
}
