//
//  crawler.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/5.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef crawler_h
#define crawler_h

#include <vector>
#include "snapshot.h"
#include "heap.h"

enum BridgeKind:uint8_t { None, gcHandle, Native, Managed, Static };

struct MemberJoint
{
    int32_t gcHandleIndex = -1;
    int32_t objectTypeIndex = -1;
    address_t objectAddress = 0;
    int32_t objectIndex = -1;
    int32_t fieldTypeIndex = -1;
    int32_t fieldIndex = -1;
    int32_t fieldOffset = 0;
    address_t fieldAddress = 0;
    int32_t arrayIndex = -1;
    bool isStatic = false;
};

struct JointBridge
{
    int32_t from = -1;
    int32_t to = -1;
    BridgeKind fromKind = BridgeKind::None;
    BridgeKind toKind = BridgeKind::None;
    int32_t jointArrayIndex = -1;
};

struct ManagedObject
{
    address_t address = 0;
    int32_t typeIndex = -1;
    int32_t managedObjectIndex = -1;
    int32_t nativeObjectIndex = -1;
    int32_t gcHandleIndex = -1;
    int32_t size = 0;
    int32_t nativeSize = 0;
    int32_t jointArrayIndex = -1;
    bool isValueType = false;
};

template <class T>
class InstanceManager
{
    std::vector<T *> __manager;
    int32_t __cursor;
    int32_t __deltaCount;
    T *__current;
    int32_t __nestCursor;
    
public:
    InstanceManager(): InstanceManager(1000) {}
    InstanceManager(int32_t deltaCount);
    
    T &add();
    T &operator[](const int32_t index);
    int32_t size();
    
    ~InstanceManager();
};

class MemorySnapshotCrawler
{
    InstanceManager<ManagedObject> managedObjects;
    InstanceManager<JointBridge> bridges;
    InstanceManager<MemberJoint> joints;
};

template<class T>
InstanceManager<T>::InstanceManager(int32_t deltaCount)
{
    __deltaCount = deltaCount;
    __current = new T[__deltaCount];
    __manager.push_back(__current);
    __nestCursor = 0;
    __cursor = 0;
}

template<class T>
T &InstanceManager<T>::add()
{
    if (__nestCursor == __deltaCount)
    {
        __current = new T[__deltaCount];
        __manager.push_back(__current);
        __nestCursor = 0;
    }
    
    __cursor += 1;
    return __current[__nestCursor++];
}

template<class T>
int32_t InstanceManager<T>::size()
{
    return __cursor;
}

template<class T>
T &InstanceManager<T>::operator[](const int32_t index)
{
    auto entity = index / __deltaCount;
    return __manager[entity][index % __deltaCount];
}

template<class T>
InstanceManager<T>::~InstanceManager<T>()
{
    for (auto i  = 0; i < __manager.size(); i++)
    {
        delete [] __manager[i];
    }
}

#endif /* crawler_h */
