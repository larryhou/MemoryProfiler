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

struct HeapSegment
{
    char *begin;
    char *end;
    
    HeapSegment(): HeapSegment(0, 0) {}
    
    HeapSegment(char *begin, char *end)
    {
        this->begin = begin;
        this->end = end;
    }
};

class HeapMemoryReader
{
    PackedMemorySnapshot &__snapshot;
    Array<MemorySection> *__managedHeapSections;
    VirtualMachineInformation *__vm;
    
protected:
    address_t __startAddress;
    address_t __stopAddress;
    const byte_t *__memory;
    int32_t __size;
    
    virtual int32_t seekOffset(address_t address);
    
public:
    HeapMemoryReader(PackedMemorySnapshot &snapshot): __snapshot(snapshot)
    {
        __managedHeapSections = snapshot.managedHeapSections;
        __vm = snapshot.virtualMachineInformation;
    }
    
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
    
    uint32_t readArrayLength(address_t address, TypeDescription &type);
    
    uint32_t readObjectSize(address_t address, TypeDescription &type);
    HeapSegment readObjectMemory(address_t address, TypeDescription &type);
    
    int32_t findHeapOfAddress(address_t address);
    
private:
    template <typename T>
    T readScalar(address_t address);
};

template <typename T>
T HeapMemoryReader::readScalar(address_t address)
{
    auto offset = seekOffset(address);
    if (offset == -1) {return 0;}
    
    return *(T *)(__memory + offset);
}

class StaticMemoryReader: public HeapMemoryReader
{
protected:
    virtual int32_t seekOffset(address_t address) override;
    
public:
    StaticMemoryReader(PackedMemorySnapshot &snapshot): HeapMemoryReader(snapshot) {}
    void load(const byte_t *bytes, int32_t size);
};

#endif /* heap_h */
