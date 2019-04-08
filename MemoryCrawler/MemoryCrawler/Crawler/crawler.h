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
#include <map>
#include "snapshot.h"
#include "heap.h"
#include "perf.h"

enum ConnectionKind:uint8_t { None, gcHandle, Native, Managed, Static };

struct MemberJoint
{
    int32_t gcHandleIndex = -1;
    int32_t hookTypeIndex = -1;
    address_t hookObjectAddress = 0;
    int32_t hookObjectIndex = -1;
    int32_t fieldTypeIndex = -1;
    int32_t fieldSlotIndex = -1;
    int32_t fieldOffset = 0;
    address_t fieldAddress = 0;
    int32_t elementArrayIndex = -1;
    int32_t jointArrayIndex = -1;
    bool isStatic = false;
};

struct JointConnection
{
    int32_t connectionArrayIndex = -1;
    int32_t from = -1;
    int32_t to = -1;
    int32_t jointArrayIndex = -1;
    ConnectionKind fromKind = ConnectionKind::None;
    ConnectionKind toKind = ConnectionKind::None;
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
    T &clone(T &item);
    int32_t size();
    
    ~InstanceManager();
};

using std::map;
using std::vector;
class MemorySnapshotCrawler
{
public:
    // crawling result
    InstanceManager<ManagedObject> managedObjects;
    InstanceManager<JointConnection> connections;
    InstanceManager<MemberJoint> joints;
    
    map<int32_t, vector<int32_t> *> fromConnections;
    map<int32_t, vector<int32_t> *> toConnections;
    
private:
    PackedMemorySnapshot &__snapshot;
    HeapMemoryReader *__memoryReader;
    VirtualMachineInformation *__vm;
    
    TimeSampler<std::nano> __sampler;
    
    // crawling map
    map<address_t, address_t> __connectionVisit;
    map<address_t, int32_t> __visit;
    
    // address map
    map<address_t, int32_t> __typeAddressMap;
    map<address_t, int32_t> __nativeObjectAddressMap;
    map<address_t, int32_t> __managedObjectAddressMap;
    map<address_t, int32_t> __managedNativeAddressMap;
    map<address_t, int32_t> __gcHandleAddressMap;

public:
    MemorySnapshotCrawler(PackedMemorySnapshot &snapshot): __snapshot(snapshot)
    {
        __memoryReader = new HeapMemoryReader(snapshot);
        __vm = snapshot.virtualMachineInformation;
    }
    
    void crawl();
    
    void tryAcceptConnection(JointConnection &connection);
    int32_t getIndexKey(ConnectionKind kind, int32_t index);
    int64_t getConnectionKey(JointConnection &connection);
    
    int32_t findTypeOfAddress(address_t address);
    int32_t findTypeAtTypeInfoAddress(address_t address);
    
    int32_t findManagedObjectOfNativeObject(address_t address);
    int32_t findManagedObjectAtAddress(address_t address);
    
    int32_t findNativeObjectAtAddress(address_t address);
    int32_t findGCHandleWithTargetAddress(address_t address);
    
    bool isSubclassOfManagedType(TypeDescription &type, int32_t baseTypeIndex);
    bool isSubclassOfNativeType(PackedNativeType &type, int32_t baseTypeIndex);
    
private:
    void initManagedTypes();
    void crawlGCHandles();
    void crawlStatic();
    
    void tryConnectWithNativeObject(ManagedObject &mo);
    
    void setObjectSize(ManagedObject &mo, TypeDescription &type, HeapMemoryReader &memoryReader);
    
    ManagedObject &createManagedObject(address_t address, int32_t typeIndex);
    
    bool isCrawlable(TypeDescription &type);
    
    void crawlManagedArrayAddress(address_t address,
                                  TypeDescription &type,
                                  HeapMemoryReader &memoryReader,
                                  MemberJoint &joint,
                                  int32_t depth);
    
    void crawlManagedEntryAddress(address_t address,
                                  TypeDescription *type,
                                  HeapMemoryReader &memoryReader,
                                  MemberJoint &joint,
                                  bool isRealType,
                                  int32_t depth);
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
T &InstanceManager<T>::clone(T &item)
{
    auto &newObject = add();
    std::memcpy(&newObject, &item, sizeof(item));
    return newObject;
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
