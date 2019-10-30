//
//  fragment.hpp
//  MemoryCrawler
//
//  Created by LARRYHOU on 2019/10/30.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef fragment_h
#define fragment_h

#include <vector>
#include "types.h"

enum ConcationType
{
    CT_IDENTICAL = 0, CT_ALLOC, CT_DEALLOC, CT_CONCAT
};

struct MemoryFragment
{
    address_t address;
    uint32_t index;
    uint32_t count;
    
    MemoryFragment() : MemoryFragment(0, 0, 0) {}
    MemoryFragment(address_t address, uint32_t count, uint32_t index): address(address), count(count), index(index) {}
};

struct MemoryConcation: public MemoryFragment
{
    std::vector<MemoryFragment> fragments;
    ConcationType type;
    
    MemoryConcation() : MemoryConcation(0, 0, 0, CT_IDENTICAL) {}
    MemoryConcation(ConcationType type) : MemoryConcation(0, 0, 0, type) {}
    MemoryConcation(address_t address, uint32_t count, uint32_t index, ConcationType type) : MemoryFragment(address, count, index), type(type) {}
};

#endif /* fragment_h */
