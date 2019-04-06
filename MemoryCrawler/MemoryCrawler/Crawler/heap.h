//
//  heap.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/6.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef heap_h
#define heap_h

#include "types.h"
#include "snapshot.h"

struct MemorySegment
{
    char *begin;
    char *end;
    
    MemorySegment(char *begin, char *end)
    {
        this->begin = begin;
        this->end = end;
    }
};

class HeapMemoryReader
{
    PackedMemorySnapshot &snapshot;
    VirtualMachineInformation *vm;
    Array<MemorySection> *managedHeapSections;
    byte_t *memory;
    address_t startAddress;
    address_t stopAddress;
    uint32_t sectionCount;
    
public:
    HeapMemoryReader(PackedMemorySnapshot &snapshot): snapshot(snapshot)
    {
        managedHeapSections = snapshot.managedHeapSections;
        vm = snapshot.virtualMachineInformation;
    }
    
    int32_t tryRead(address_t address);
    
    int8_t readInt8(address_t address) { return readScalar<int8_t>(address); }
    int16_t readInt16(address_t address)  { return readScalar<int16_t>(address); }
    int32_t readInt32(address_t address)  { return readScalar<int32_t>(address); }
    int64_t readInt64(address_t address)  { return readScalar<int64_t>(address); }
    
    uint8_t readUInt8(address_t address) { return readScalar<uint8_t>(address); }
    uint16_t readUInt16(address_t address) { return readScalar<uint16_t>(address); }
    uint32_t readUInt32(address_t address) { return readScalar<uint32_t>(address); }
    uint64_t readUInt64(address_t address) { return readScalar<uint64_t>(address); }
    
    bool readBoolean(address_t address) { return readScalar<bool>(address); }
    
    float readFloat(address_t address) { return readScalar<float>(address); }
    double readDouble(address_t address) { return readScalar<double>(address); }
    
    address_t readPointer(address_t address) { return readScalar<address_t>(address); }
    
    uint32_t readStringLength(address_t address);
    int32_t readString(address_t address, char16_t *buffer);
    
    uint32_t readArrayLength(address_t address);
    
    uint32_t readObjectSize(address_t address);
    MemorySegment readObjectMemory(address_t address);
    
    int32_t findHeapOfAddress(address_t address);
private:
    template <typename T>
    T readScalar(address_t address);
};

template <typename T>
T HeapMemoryReader::readScalar(address_t address)
{
    auto offset = tryRead(address);
    if (offset == -1) {return 0;}
    
    return *(T *)(memory + offset);
}

#endif /* heap_h */
