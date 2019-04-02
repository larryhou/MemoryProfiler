//
//  main.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/2.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include <iostream>
#include "FieldDescription.h"
using namespace std;

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
    
    return 0;
}
