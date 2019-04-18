//
//  crawler.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/5.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef crawler_h
#define crawler_h

#include <set>
#include <fstream>
#include <vector>
#include <map>
#include <unordered_map>
#include "snapshot.h"
#include "heap.h"
#include "perf.h"
#include "serialize.h"

using std::vector;
using std::set;

struct EntityJoint
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
    int32_t jointEntryIndex = -1;
    bool isStatic = false;
    bool isConnected = false;
};

struct EntityConnection: public Connection
{
    int32_t jointArrayIndex = -1;
    int32_t jointEntryIndex = -1;
};

struct ManagedObject
{
    std::vector<int32_t> fromConnections;
    std::vector<int32_t> toConnections;
    
    address_t address = 0;
    int32_t typeIndex = -1;
    int32_t managedObjectIndex = -1;
    int32_t nativeObjectIndex = -1;
    int32_t size = 0;
    int32_t nativeSize = 0;
    bool isValueType = false;
};

#include <memory>
#include <new>

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
    void rollback();
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
    InstanceManager<EntityConnection> connections;
    InstanceManager<EntityJoint> joints;
    
    PackedMemorySnapshot snapshot;
    
private:
    static constexpr int32_t CRAWLER_ITERATE_CAPACITY = 100000;
    
    HeapMemoryReader *__memoryReader;
    StaticMemoryReader *__staticMemoryReader;
    VirtualMachineInformation *__vm;
    
    TimeSampler<std::nano> __sampler;
    address_t *__mirror = nullptr;
    
    // crawling map
    map<address_t, int32_t> __crawlingVisit;
    
    // address map
    std::unordered_map<address_t, int32_t> __typeAddressMap;
    std::unordered_map<address_t, int32_t> __nativeObjectAddressMap;
    map<address_t, int32_t> __managedObjectAddressMap;
    
    map<address_t, int32_t> __managedNativeAddressMap;
    map<address_t, int32_t> __nativeManagedAddressMap;

public:
    MemorySnapshotCrawler();
    MemorySnapshotCrawler(const char *filepath);
    
    MemorySnapshotCrawler &crawl();
    
    const char16_t *getString(address_t address, int32_t &size);
    
    void tryAcceptConnection(EntityConnection &connection);
    void tryAcceptConnection(Connection &connection);
    
    void dumpMRefChain(address_t address, bool includeCircular, int32_t limit = 2);
    void dumpNRefChain(address_t address, bool includeCircular, int32_t limit = 2);
    vector<vector<int32_t>> iterateNRefChain(PackedNativeUnityEngineObject *no,
                                             vector<int32_t> chain, set<int64_t> antiCircular, int32_t limit = 2, int32_t __iter_capacity = 1);
    vector<vector<int32_t>> iterateMRefChain(ManagedObject *mo,
                                             vector<int32_t> chain, set<int64_t> antiCircular, int32_t limit = 2, int32_t __iter_capacity = 1);
    
    address_t findMObjectOfNObject(address_t address);
    address_t findNObjectOfMObject(address_t address);
    
    ~MemorySnapshotCrawler();
    
private:
    void prepare();
    void crawlGCHandles();
    void crawlStatic();
    void debug();
    
    int32_t findTypeOfAddress(address_t address);
    int32_t findTypeAtTypeAddress(address_t address);
    
    int32_t findMObjectAtAddress(address_t address);
    int32_t findNObjectAtAddress(address_t address);
    
    bool isSubclassOfMType(TypeDescription &type, int32_t baseTypeIndex);
    bool isSubclassOfNType(PackedNativeType &type, int32_t baseTypeIndex);
    
    void tryConnectWithNativeObject(ManagedObject &mo);
    
    ManagedObject &createManagedObject(address_t address, int32_t typeIndex);
    
    bool isCrawlable(TypeDescription &type);
    
    bool crawlManagedArrayAddress(address_t address,
                                  TypeDescription &type,
                                  HeapMemoryReader &memoryReader,
                                  EntityJoint &joint,
                                  int32_t depth);
    
    bool crawlManagedEntryAddress(address_t address,
                                  TypeDescription *type,
                                  HeapMemoryReader &memoryReader,
                                  EntityJoint &joint,
                                  bool isRealType,
                                  int32_t depth);
    
    void summarize();
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
        auto entity = __cursor / __deltaCount;
        if (entity < __manager.size())
        {
            __current = __manager[entity];
        }
        else
        {
            __current = new T[__deltaCount];
            __manager.push_back(__current);
        }
        
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
void InstanceManager<T>::rollback()
{
    if (__cursor <= 0) {return;}
    
    __cursor -= 1;
    if (__nestCursor > 0)
    {
        __nestCursor -= 1;
    }
    else
    {
        __nestCursor = __deltaCount - 1;
    }
    
    auto *ptr = &(*this)[__cursor];
    memset(ptr, 0, sizeof(T));
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
    for (auto iter = __manager.begin(); iter != __manager.end(); iter++)
    {
        delete [] *iter;
    }
}

#endif /* crawler_h */
