//
//  types.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/3.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef types_h
#define types_h
#include <ios>
#include <string>

using byte_t = unsigned char;
using address_t = int64_t;
using seekdir_t = std::ios_base::seekdir;
using unicode_t = std::u16string;

template <typename T>
struct Array
{
    uint32_t size;
    T *items;
    
    Array(uint32_t size) { items = new T[size]; }
    T &operator[](const int32_t index) { return items[index]; }
    ~Array() { delete [] items; }
};

#endif /* types_h */
