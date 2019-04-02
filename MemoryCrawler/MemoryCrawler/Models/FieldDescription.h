//
//  FieldDescription.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/2.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef FieldDescription_h
#define FieldDescription_h

struct FieldDescription
{
    int32_t offset;
    int32_t typeIndex;
    const char* name;
    int32_t hostTypeIndex;
    int16_t slotIndex;
    bool isStatic;
};

#endif /* FieldDescription_h */

