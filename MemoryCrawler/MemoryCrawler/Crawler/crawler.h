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
#include <locale>
#include <codecvt>
#include <fstream>
#include <vector>
#include <map>
#include <unordered_map>
#include "snapshot.h"
#include "heap.h"
#include "perf.h"
#include "serialize.h"
#include "stat.h"

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
    MemoryState state = MS_none;
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
    static constexpr int64_t REF_ITERATE_CAPACITY = 1 << 20;
    static constexpr int32_t REF_ITERATE_DEPTH = 32;
    static constexpr int32_t SEP_DASH_COUNT = 40;
    
    HeapMemoryReader *__memoryReader;
    StaticMemoryReader *__staticMemoryReader;
    VirtualMachineInformation *__vm;
    
    std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> __convertor;
    
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
    
    HashCaculator __hash;

public:
    MemorySnapshotCrawler();
    MemorySnapshotCrawler(const char *filepath);
    
    MemorySnapshotCrawler &crawl();
    
    const char16_t *getString(address_t address, int32_t &size);
    const string getUTFString(address_t address, int32_t &size, bool compactMode = false);
    
    void tryAcceptConnection(EntityConnection &connection);
    void tryAcceptConnection(Connection &connection);
    
    void trackMStatistics(MemoryState state, int32_t depth = 5);
    void trackNStatistics(MemoryState state, int32_t depth = 5);
    
    void trackMTypeObjects(MemoryState state, int32_t typeIndex);
    void trackNTypeObjects(MemoryState state, int32_t typeIndex);
    
    void findMObject(address_t address);
    void findNObject(address_t address);
    
    void dumpRedundants(int32_t typeIndex);
    
    void dumpUnbalancedEvents(MemoryState state);
    
    void inspectMType(int32_t typeIndex);
    void inspectNType(int32_t typeIndex);
    
    void inspectMObject(address_t address, int32_t depth = 0);
    void inspectNObject(address_t address, int32_t depth = 0);
    void dumpMObjectHierarchy(address_t address, TypeDescription *type,
                              set<int64_t> antiCircular, bool isActualType, int32_t limit, const char *indent, int32_t __iter_depth = 0);
    void dumpNObjectHierarchy(PackedNativeUnityEngineObject *no,
                              set<int64_t> antiCircular, int32_t limit, const char *indent, int32_t __iter_depth = 0, int32_t __iter_capacity = 1);
    
    void barMMemory(MemoryState state, int32_t rank = 20);
    void barNMemory(MemoryState state, int32_t rank = 20);
    
    void statHeap(int32_t rank = 20);
    
    void dumpMRefChain(address_t address, bool includeCircular, int32_t limit = 2);
    void dumpNRefChain(address_t address, bool includeCircular, int32_t limit = 2);
    vector<vector<int32_t>> iterateNRefChain(PackedNativeUnityEngineObject *no,
                                             vector<int32_t> chain, set<int64_t> antiCircular, int32_t limit = 2, int64_t __iter_capacity = 1, int32_t __depth = 0);
    vector<vector<int32_t>> iterateMRefChain(ManagedObject *mo,
                                             vector<int32_t> chain, set<int64_t> antiCircular, int32_t limit = 2, int64_t __iter_capacity = 1, int32_t __depth = 0);
    
    address_t findMObjectOfNObject(address_t address);
    address_t findNObjectOfMObject(address_t address);
    
    void compare(MemorySnapshotCrawler &crawler);
    
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
    
    bool deriveFromMType(TypeDescription &type, int32_t baseTypeIndex);
    bool deriveFromNType(PackedNativeType &type, int32_t baseTypeIndex);
    
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
    bool isPremitiveType(int32_t typeIndex);
    void dumpPremitiveValue(address_t address, int32_t typeIndex);
    void dumpByteArray(const char *data, int32_t size);
    
    
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
