//
//  types.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/5/16.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "types.h"

const char HashCaculator::__hex_map[] = "0123456789abcdef";

size_t HashCaculator::get(const char *data, const CC_LONG size)
{
    CC_MD5(data, size, __digest);
    
    char *ptr = __hexdigest;
    auto *end = __digest + 16;
    for (auto i = __digest; i != end; i++)
    {
        auto v = *i;
        *ptr++ = __hex_map[v >> 4];
        *ptr++ = __hex_map[v & 0xF];
    }
    *ptr = 0;
    
    return __hash(__hexdigest);
}
