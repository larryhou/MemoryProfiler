//
//  main.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/2.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include <iostream>
#include <vector>
#include "Models/snapshot.h"

using std::cout;
using std::endl;
using std::vector;

int main(int argc, const char * argv[])
{
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
    return 0;
}
