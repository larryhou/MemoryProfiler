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
    uint32_t uid = 0;
    
    bool isStatic = false;
};

struct JointBridge
{
    int32_t from = -1;
    int32_t to = -1;
    BridgeKind fromKind = BridgeKind::None;
    BridgeKind toKind = BridgeKind::None;
    int32_t jointArrayIndex = -1;
    uint32_t uid = 0;
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

class ManagedObjectManager
{
    
};

#endif /* crawler_h */
