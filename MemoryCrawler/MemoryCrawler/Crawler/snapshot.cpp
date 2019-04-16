//
//  snapshot.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/3.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include <stdio.h>
#include "snapshot.h"

Connection::~Connection()
{
    
}

FieldDescription::~FieldDescription()
{
    
}

TypeDescription::~TypeDescription()
{
    delete fields;
    delete staticFieldBytes;
}

MemorySection::~MemorySection()
{
     delete bytes;
}

PackedGCHandle::~PackedGCHandle()
{
    
}

PackedNativeUnityEngineObject::~PackedNativeUnityEngineObject()
{
    
}

PackedNativeType::~PackedNativeType()
{
    
}

VirtualMachineInformation::~VirtualMachineInformation()
{
    
}

PackedMemorySnapshot::~PackedMemorySnapshot()
{
    delete connections;
    delete gcHandles;
    delete managedHeapSections;
    delete nativeObjects;
    delete nativeTypes;
    delete typeDescriptions;
    delete sortedHeapSections;
}

