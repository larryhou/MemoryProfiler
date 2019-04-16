//
//  leak.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/16.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef leak_h
#define leak_h

#include <string>
using std::strcmp;

template <class T>
void inspectCondition(const char *condition)
{
    if (0 == strcmp(condition, "array"))
    {
        Array<T> array(1000000);
    }
    else if (0 == strcmp(condition, "im"))
    {
        InstanceManager<T> manager(10000);
        for (auto i = 0; i < 1000000; i++) { manager.add(); }
    }
    else if (0 == strcmp(condition, "vector"))
    {
        std::vector<T> vector(1000000);
    }
    else if (0 == strcmp(condition, "carray"))
    {
        auto array = new T[1000000];
        delete [] array;
    }
    else if (0 == strcmp(condition, "t1"))
    {
        std::vector<const T *> vector;
        for (auto i = 0; i < 1000; i++)
        {
            vector.push_back(new T[1000]);
        }
        
        for (auto iter = vector.begin(); iter != vector.end(); iter++)
        {
            delete [] *iter;
        }
    }
    else if (0 == strcmp(condition, "t2"))
    {
        for (auto i = 0; i < 1000; i++)
        {
            std::allocator<T> allocator;
            auto *ptr = allocator.allocate(1000);
            allocator.deallocate(ptr, 1000);
        }
    }
}

#endif /* leak_h */
