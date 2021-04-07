//
//  format.hpp
//  MemoryCrawler
//
//  Created by LARRYHOU on 2019/11/5.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef format_h
#define format_h

#include <stdio.h>
#include <fstream>
#include "snapshot.h"
#include "stream.h"
#include "types.h"

class HeapExplorerFormat
{
    FileStream __fs;
    
public:
    
    HeapExplorerFormat(){}
    
    void encode(PackedMemorySnapshot *snapshot, const char* filename)
    {
        __fs.open(filename, std::fstream::out | std::fstream::trunc);
        
        encodeHeader();
        encode(*snapshot->nativeTypes);
        encode(*snapshot->nativeObjects);
        encode(*snapshot->gcHandles);
        encode(*snapshot->connections);
        encode(*snapshot->heapSections);
        encode(*snapshot->typeDescriptions);
        encode(snapshot->virtualMachineInformation);
        
        __fs.close();
    }
private:
    void encodeHeader();
    void encode(Array<PackedNativeType> &nativeTypes);
    void encode(Array<PackedNativeUnityEngineObject> &nativeObjects);
    void encode(Array<PackedGCHandle> &gcHandles);
    void encode(Array<Connection> &connections);
    void encode(Array<MemorySection> &heapSections);
    void encode(Array<TypeDescription> &typeDescriptions);
    void encode(VirtualMachineInformation &vm);
};


#endif /* format_h */
