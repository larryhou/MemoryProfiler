//
//  heap.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/6.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef heap_h
#define heap_h

#include <cstring>
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
    std::vector<MemorySection *> *__sortedHeapSections;
    VirtualMachineInformation *__vm;
    
protected:
    address_t __startAddress = 0;
    address_t __stopAddress = 0;
    const byte_t *__memory = nullptr;
    int32_t __size = 0;
    
    virtual int32_t seekOffset(address_t address);
    
public:
    HeapMemoryReader(PackedMemorySnapshot &snapshot): __snapshot(snapshot)
    {
        __sortedHeapSections = snapshot.sortedHeapSections;
        
        __vm = &snapshot.virtualMachineInformation;
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
    
    address_t readPointer(address_t address)
    {
        return __vm->pointerSize == 8 ? readScalar<address_t>(address) : (address_t)readScalar<uint32_t>(address);
    }
    
    uint32_t readStringLength(address_t address);
    int32_t readString(address_t address, char16_t *buffer);
    const char16_t *readString(address_t address, int32_t &size);
    
    uint32_t readArrayLength(address_t address, TypeDescription &type);
    
    uint32_t readObjectSize(address_t address, TypeDescription &type);
    HeapSegment readObjectMemory(address_t address, TypeDescription &type);
    const char *readMemory(address_t address);
    
    int32_t findHeapOfAddress(address_t address);
    virtual bool isStatic();
    
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
    void load(const Array<byte_t> &data);
    virtual bool isStatic() override;
};

#endif /* heap_h */
