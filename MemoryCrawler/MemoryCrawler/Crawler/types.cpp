//
//  types.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/5/16.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "types.h"
#include "MD5.h"

const char HashCaculator::__hex_map[] = "0123456789abcdef";

size_t HashCaculator::get(const char *data, size_t size)
{
    MD5 md5;
    md5.update(data, size);
    md5.finalize();

    return __hash(md5.hexdigest());
}
