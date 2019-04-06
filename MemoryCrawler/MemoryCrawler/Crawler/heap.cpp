//
//  heap.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/6.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "heap.h"

int32_t HeapMemoryReader::tryRead(address_t address)
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
    
    return (int32_t)(address - startAddress);
}

uint32_t HeapMemoryReader::readStringLength(address_t address)
{
    auto offset = tryRead(address);
    if (offset == -1) {return 0;}
    
    auto length = readInt32(address);
    return length > 0 ? length : 0;
}

int32_t HeapMemoryReader::readString(address_t address, char16_t *buffer)
{
    auto offset = tryRead(address);
    if (offset == -1) {return -1;}
    
    auto length = readInt32(address);
    if (length <= 0) {return -2;}
    
    offset += 4;
    memcpy(buffer, memory + offset, length << 1);
    return length;
}

//uint32_t HeapMemoryReader::readArrayLength(address_t address)
//{
//
//}

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
