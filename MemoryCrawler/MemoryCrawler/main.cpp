//
//  main.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/2.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include <iostream>
#include <vector>
#include "Crawler/snapshot.h"
#include "Crawler/stream.h"

using std::cout;
using std::endl;
using std::vector;

void checkEndian()
{
    int16_t num = 0x01;
    auto ptr = &num;
    auto btr = (byte_t*)ptr;
    bool isLittleEndian = *btr == 0x01;
    cout << std::hex << "num=0x" << num
    << " [0]=0x" << (int)*btr
    << " [2]=0x" << (int)*(btr+1)
    << " [3]=0x" << (int)*(btr+2)
    << " [4]=0x" << (int)*(btr+3)
    << " isLittleEndian=" << isLittleEndian
    << endl;
}

void decodeFloat()
{
    int32_t num = 0x4047e313;
    auto ptr = (float*)&num;
    cout << std::hex << "float from int32_t bytes=0x" << num << " value=" << *ptr << endl;
    
    byte_t data[4] = {0x13, 0xe3, 0x47, 0x40};
    ptr = (float*)data;
    cout << std::hex << "float from byte[4] bytes=0x" << (int)data[0] << " value=" << *ptr << endl;
}

void decodeDouble()
{
    byte_t data[] = {0x6e, 0xa7, 0x75, 0x67, 0x62, 0xfc, 0x08, 0x40, 0x00};
    auto ptr = (double *)data;
    auto asign = (byte_t *)data;
    cout << std::hex << "double from byte[8] bytes=0x" << (int)data[0] << " value=" << *ptr
    << " size=" << sizeof(data) << " ptr_size=" << sizeof(asign)
    << endl;
}

int32_t swap(int32_t value)
{
    int32_t v;
    auto raw = (char *)&value;
    auto ptr = (char *)&v;
    ptr[0] = raw[3];
    ptr[1] = raw[2];
    ptr[2] = raw[1];
    ptr[3] = raw[0];
    return v;
}

void testStream(const char * filepath)
{
    FileStream fs;
    fs.open(filepath);
    auto mime = fs.readString(3);
    cout << "MIME=" << mime << endl
    << "description=" << fs.readString(swap(fs.readUInt32())) << endl
    << "unity_version=" << fs.readString(swap(fs.readUInt32())) << endl
    << "system_version=" << fs.readString(swap(fs.readUInt32())) << endl
    << "uuid=" << fs.readUUID() << endl;
    
    cout << "size=" << swap(fs.readUInt32())
    << endl;
}

int main(int argc, const char * argv[])
{
    cout << "argc=" << argc << endl;
    for (auto i = 0; i < argc; i++)
    {
        cout << "argv[" << i << "]=" << argv[i] << endl;
    }
    cout << "==================" << endl;
    if (argc > 1)
    {
        testStream(argv[1]);
    }
    
//    playground();
    
    return 0;
}

void playground()
{
    checkEndian();
    decodeFloat();
    decodeDouble();
    
    // insert code here...
    cout << "Hello, World!\n";
    int intType = 0;
    uint uintType = 0;
    int64_t int64Type = 0;
    int32_t int32Type = 0;
    cout << "int=" << sizeof(intType)
    << ", uint=" << sizeof(uintType)
    << ", int64_t=" << sizeof(int64Type)
    << ", int32_t=" << sizeof(int32Type)
    << ", wchar_t=" << sizeof(char16_t)
    << endl;
    
    FieldDescription field;
    cout
    << "field offset=" << field.offset
    << ", size=" << sizeof(field)
    << ", bool=" << sizeof(bool)
    << ", pointer=" << sizeof(&field)
    << endl;
    
    cout
    << "name=" << (uint64_t)&field.name - (uint64_t)&field
    << " isStatic=" << (uint64_t)&field.isStatic - (uint64_t)&field
    << " offset=" << (uint64_t)&field.offset - (uint64_t)&field
    << " typeIndex=" << (uint64_t)&field.typeIndex - (uint64_t)&field
    << " hostTypeIndex=" << (uint64_t)&field.hostTypeIndex - (uint64_t)&field
    << " slotIndex=" << (uint64_t)&field.slotIndex - (uint64_t)&field
    << endl;
    
    cout << "TypeDescription size=" << sizeof(TypeDescription) << endl;
    
    size_t instanceCount = 8000000;
    auto fieldArray = new FieldDescription[instanceCount];
    for (auto i = 0; i < instanceCount; ++i)
    {
        fieldArray[i].typeIndex = i;
    }
    
    cout << "field count=" << instanceCount
    << "\nptr = " << &fieldArray
    << "\n[0] = " << &fieldArray[0]
    << "\n[1] = " << &fieldArray[1]
    << "\n[2] = " << &fieldArray[2]
    << "\n[3] = " << &fieldArray[3]
    << "\n[4] = " << &*(fieldArray+4)
    << endl;
}
