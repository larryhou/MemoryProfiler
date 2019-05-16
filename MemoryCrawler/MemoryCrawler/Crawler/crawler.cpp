//
//  crawler.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/5.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "crawler.h"

MemorySnapshotCrawler::MemorySnapshotCrawler(const char *filepath)
{
    MemorySnapshotReader(filepath).read(snapshot);
    
    __memoryReader = new HeapMemoryReader(snapshot);
    __staticMemoryReader = new StaticMemoryReader(snapshot);
    __vm = &snapshot.virtualMachineInformation;
    debug();
}

MemorySnapshotCrawler::MemorySnapshotCrawler()
{
    __memoryReader = new HeapMemoryReader(snapshot);
    __staticMemoryReader = new StaticMemoryReader(snapshot);
    __vm = &snapshot.virtualMachineInformation;
    debug();
}

MemorySnapshotCrawler &MemorySnapshotCrawler::crawl()
{
    __sampler.begin("MemorySnapshotCrawler");
    prepare();
    crawlGCHandles();
    crawlStatic();
    summarize();
    __sampler.end();
    __sampler.summarize();
    return *this;
}

using std::ifstream;
void MemorySnapshotCrawler::debug()
{
#if USE_ADDRESS_MIRROR
    ifstream fs;
    fs.open("/Users/larryhou/Documents/MemoryProfiler/address.bin", ifstream::in | ifstream::binary);
    
    int32_t size = 0;
    char *ptr = (char *)&size;
    fs.read(ptr, 4);
    
    __mirror = new address_t[size];
    fs.read((char *)__mirror, size * sizeof(address_t));
    fs.close();
#endif
}

void MemorySnapshotCrawler::summarize()
{
    __sampler.begin("summarize_managed_objects");
    auto &nativeObjects = *snapshot.nativeObjects;
    auto &typeDescriptions = *snapshot.typeDescriptions;
    for (auto i = 0; i < typeDescriptions.size; i++)
    {
        auto &type = typeDescriptions[i];
        type.instanceMemory = 0;
        type.instanceCount = 0;
        type.nativeMemory = 0;
    }
    
    for (auto i = 0; i < managedObjects.size(); i++)
    {
        auto &mo = managedObjects[i];
        auto &type = typeDescriptions[mo.typeIndex];
        type.instanceMemory += mo.size;
        type.instanceCount += 1;
        
        if (mo.nativeObjectIndex >= 0)
        {
            auto &no = nativeObjects[mo.nativeObjectIndex];
            type.nativeMemory += no.size;
        }
    }
    __sampler.end();
}

void MemorySnapshotCrawler::prepare()
{
    __sampler.begin("prepare");
    __sampler.begin("init_managed_types");
    Array<TypeDescription> &typeDescriptions = *snapshot.typeDescriptions;
    for (auto i = 0; i < typeDescriptions.size; i++)
    {
        TypeDescription &type = typeDescriptions[i];
        type.isUnityEngineObjectType = isSubclassOfMType(type, snapshot.managedTypeIndex.unityengine_Object);
    }
    __sampler.end();
    __sampler.begin("init_native_connections");
    
    auto offset = snapshot.gcHandles->size;
    auto &nativeConnections = *snapshot.connections;
    for (auto i = 0; i < nativeConnections.size; i++)
    {
        auto &nc = nativeConnections[i];
        nc.connectionArrayIndex = i;
        if (nc.from >= offset)
        {
            nc.fromKind = CK_native;
            nc.from -= offset;
        }
        else
        {
            nc.fromKind = CK_gcHandle;
        }
        
        if (nc.to >= offset)
        {
            nc.toKind = CK_native;
            nc.to -= offset;
        }
        else
        {
            nc.toKind = CK_gcHandle;
        }
        
        tryAcceptConnection(nc);
    }
    
    __sampler.end();
    __sampler.end();
}

void MemorySnapshotCrawler::compare(MemorySnapshotCrawler &crawler)
{
    map<address_t, int64_t> container;
    
    // Compare managed objects
    for (auto i = 0; i < crawler.managedObjects.size(); i++)
    {
        auto &mo = crawler.managedObjects[i];
        if (!mo.isValueType)
        {
            auto &type = crawler.snapshot.typeDescriptions->items[mo.typeIndex];
            container.insert(pair<address_t, int64_t>(mo.address, type.typeInfoAddress));
        }
    }
    
    for (auto i = 0; i < managedObjects.size(); i++)
    {
        auto &mo = managedObjects[i];
        if (!mo.isValueType)
        {
            auto &type = snapshot.typeDescriptions->items[mo.typeIndex];
            auto iter = container.find(mo.address);
            mo.state = (iter == container.end() || iter->second != type.typeInfoAddress)? MS_allocated : MS_persistent;
        }
        else
        {
            mo.state = MS_none;
        }
    }
    
    // Compare native objects
    container.clear();
    for (auto i = 0; i < crawler.snapshot.nativeObjects->size; i++)
    {
        auto &no = crawler.snapshot.nativeObjects->items[i];
        container.insert(pair<address_t, int32_t>(no.nativeObjectAddress, no.nativeTypeArrayIndex));
    }
    
    for (auto i = 0; i < snapshot.nativeObjects->size; i++)
    {
        auto &no = snapshot.nativeObjects->items[i];
        auto iter = container.find(no.nativeObjectAddress);
        
        if (iter == container.end())
        {
            no.state = MS_allocated;
        }
        else
        {
            auto &typeA = crawler.snapshot.nativeTypes->items[iter->second];
            auto &typeB = snapshot.nativeTypes->items[no.nativeTypeArrayIndex];
            no.state = *typeA.name == *typeB.name ? MS_persistent : MS_allocated;
        }
    }
}

void MemorySnapshotCrawler::trackMStatistics(MemoryState state, int32_t depth)
{
    TrackStatistics objects;
    for (auto i = 0; i < managedObjects.size(); i++)
    {
        auto &mo = managedObjects[i];
        if (state == MS_none || mo.state == state)
        {
            objects.collect(i, mo.typeIndex, mo.size);
        }
    }
    
    char sep[3*SEP_DASH_COUNT + 1];
    memset(sep, 0, sizeof(sep));
    
    char *iter = sep;
    for (auto i = 0; i < SEP_DASH_COUNT; i++)
    {
        memcpy(iter, "─", 3);
        iter += 3;
    }
    
    int32_t total = 0;
    int32_t count = 0;
    objects.summarize();
    printf("┌%s\n", sep);
    objects.foreach([&](int32_t itemIndex, int32_t typeIndex, int32_t size, uint64_t detail)
                    {
                        auto &type = snapshot.typeDescriptions->items[typeIndex];
                        switch (itemIndex)
                        {
                            case -1:
                            {
                                printf("│ [%s] memory=%d type_index=%d\n", type.name->c_str(), (int32_t)size, type.typeIndex);
                                break;
                            }
                            case -2:
                            {
                                auto skipCount = detail >> 32;
                                auto typeCount = detail & 0xFFFFFFFF;
                                if (skipCount > 0){printf("│ [~] %d/%d=%d\n", (uint32_t)skipCount, (uint32_t)typeCount, size);}
                                total += size;
                                count += skipCount;
                                printf("├%s\n", sep);
                                break;
                            }
                            default:
                            {
                                total += size;
                                count ++;
                                auto &mo = managedObjects[itemIndex];
                                printf("│ 0x%08llx %6d %s\n", mo.address, mo.size, type.name->c_str());
                                break;
                            }
                        }
                    }, depth);
    printf("\e[37m│ [SUMMARY] count=%d memory=%d\n", count, total);
    printf("└%s\n", sep);
}

void MemorySnapshotCrawler::trackNStatistics(MemoryState state, int32_t depth)
{
    int32_t size = 1;
    TrackStatistics objects;
    for (auto i = 0; i < snapshot.nativeObjects->size; i++)
    {
        auto &no = snapshot.nativeObjects->items[i];
        if (state == MS_none || no.state == state)
        {
            objects.collect(i, no.nativeTypeArrayIndex, no.size);
            size = std::max(size, no.size);
        }
    }
    
    char sep[3*SEP_DASH_COUNT + 1];
    memset(sep, 0, sizeof(sep));
    
    char *iter = sep;
    for (auto i = 0; i < SEP_DASH_COUNT; i++)
    {
        memcpy(iter, "─", 3);
        iter += 3;
    }
    
    auto digits = (int32_t)std::ceil(std::log10(size));
    
    char format[32];
    sprintf(format, "│ 0x%%08llx %%%dd '%%s'\n", digits);
    objects.summarize();
    
    int32_t total = 0;
    int32_t count = 0;
    printf("┌%s\n", sep);
    objects.foreach([&](int32_t itemIndex, int32_t typeIndex, int32_t size, uint64_t detail)
                    {
                        auto &type = snapshot.nativeTypes->items[typeIndex];
                        switch (itemIndex)
                        {
                            case -1:
                            {
                                printf("│ [%s] memory=%d type_index=%d\n", type.name->c_str(), (int32_t)size, type.typeIndex);
                                break;
                            }
                            case -2:
                            {
                                auto skipCount = detail >> 32;
                                auto typeCount = detail & 0xFFFFFFFF;
                                if (skipCount > 0){printf("│ [~] %d/%d=%d\n", (uint32_t)skipCount, (uint32_t)typeCount, size);}
                                total += size;
                                count += skipCount;
                                printf("├%s\n", sep);
                                break;
                            }
                            default:
                            {
                                total += size;
                                count ++;
                                auto &no = snapshot.nativeObjects->items[itemIndex];
                                printf(format, no.nativeObjectAddress, no.size, no.name->c_str());
                                break;
                            }
                        }
                    }, depth);
    printf("\e[37m│ [SUMMARY] count=%d memory=%d\n", count, total);
    printf("└%s\n", sep);
}

void MemorySnapshotCrawler::trackMTypeObjects(MemoryState state, int32_t typeIndex)
{
    TrackStatistics objects;
    for (auto i = 0; i < managedObjects.size(); i++)
    {
        auto &mo = managedObjects[i];
        if (mo.typeIndex == typeIndex && (state == MS_none || state == mo.state))
        {
            objects.collect(i, mo.typeIndex, mo.size);
        }
    }
    
    objects.summarize(true);
    
    int32_t total = 0;
    int32_t count = 0;
    objects.foreach([&](int32_t itemIndex, int32_t typeIndex, int32_t size, uint64_t detail)
                    {
                        auto &type = snapshot.typeDescriptions->items[typeIndex];
                        if (itemIndex < 0)
                        {
                            if (itemIndex == -1){printf("[%s][=] memory=%d\n", type.name->c_str(), (int32_t)size);}
                            return;
                        }
                        count++;
                        total += size;
                        auto &mo = managedObjects[itemIndex];
                        printf("0x%08llx %8d %s\n", mo.address, mo.size, type.name->c_str());
                    }, 0);
    printf("\e[37m[SUMMARY] count=%d memory=%d\n", count, total);
}

void MemorySnapshotCrawler::trackNTypeObjects(MemoryState state, int32_t typeIndex)
{
    TrackStatistics objects;
    for (auto i = 0; i < snapshot.nativeObjects->size; i++)
    {
        auto &no = snapshot.nativeObjects->items[i];
        if (no.nativeTypeArrayIndex == typeIndex && (state == MS_none || state == no.state))
        {
            objects.collect(i, no.nativeTypeArrayIndex, no.size);
        }
    }
    
    objects.summarize(true);
    
    int32_t total = 0;
    int32_t count = 0;
    objects.foreach([&](int32_t itemIndex, int32_t typeIndex, int32_t size, uint64_t detail)
                    {
                        auto &type = snapshot.nativeTypes->items[typeIndex];
                        if (itemIndex < 0)
                        {
                            if (itemIndex == -1){printf("[%s][=] memory=%d\n", type.name->c_str(), (int32_t)size);}
                            return;
                        }
                        count++;
                        total += size;
                        auto &no = snapshot.nativeObjects->items[itemIndex];
                        printf("0x%08llx %8d '%s'\n", no.nativeObjectAddress, no.size, no.name->c_str());
                    }, 0);
    printf("\e[37m[SUMMARY] count=%d memory=%d\n", count, total);
}

void MemorySnapshotCrawler::barMMemory(MemoryState state, int32_t rank)
{
    double totalMemory = 0;
    vector<int32_t> indice;
    map<int32_t, int32_t> typeMemory;
    map<int32_t, int32_t> typeCount;
    
    for (auto i = 0; i < managedObjects.size(); i++)
    {
        auto &mo = managedObjects[i];
        if (state == MS_none || mo.state == state)
        {
            totalMemory += mo.size;
            auto typeIndex = mo.typeIndex;
            auto iter = typeMemory.find(typeIndex);
            if (iter == typeMemory.end())
            {
                iter = typeMemory.insert(pair<int32_t, int32_t>(typeIndex, 0)).first;
                indice.push_back(typeIndex);
                
                typeCount.insert(pair<int32_t, int32_t>(typeIndex, 0));
            }
            
            iter->second += mo.size;
            typeCount.at(typeIndex)++;
        }
    }
    
    std::sort(indice.begin(), indice.end(), [&](int32_t a, int32_t b)
              {
                  auto ma = typeMemory[a];
                  auto mb = typeMemory[b];
                  if (ma != mb) {return ma > mb;}
                  return typeCount.at(a) < typeCount.at(b);
              });
    
    vector<std::tuple<int32_t, double, double>> stats;
    
    double percent = 0;
    double accumulation = 0;
    for (auto i = 0; i < indice.size(); i++)
    {
        auto index = indice[i];
        percent = 100 * (typeMemory.at(index) / totalMemory);
        accumulation += percent;
        stats.push_back(std::make_tuple(index, percent, accumulation));
    }
    
    char progress[300+1];
    char fence[] = "█";
    auto &managedTypes = *snapshot.typeDescriptions;
    for (auto i = 0; i < stats.size(); i++)
    {
        if (rank > 0 && i >= rank){break;}
        auto &item = stats[i];
        auto typeIndex = std::get<0>(item);
        auto &type = managedTypes[typeIndex];
        memset(progress, 0, sizeof(progress));
        auto count = std::max(1, (int32_t)std::round(std::get<1>(item)));
        printf("%6.2f %6.2f ", std::get<1>(item), std::get<2>(item));
        char *iter = progress;
        for (auto n = 0; n < count; n++)
        {
            memcpy(iter, fence, 3);
            iter += 3;
        }
        printf("%s %s %'d #%d *%d\n", progress, type.name->c_str(), typeMemory.at(typeIndex), typeCount.at(typeIndex), typeIndex);
    }
}

void MemorySnapshotCrawler::barNMemory(MemoryState state, int32_t rank)
{
    double totalMemory = 0;
    vector<int32_t> indice;
    map<int32_t, int32_t> typeMemory;
    map<int32_t, int32_t> typeCount;
    
    auto &nativeObjects = *snapshot.nativeObjects;
    for (auto i = 0; i < nativeObjects.size; i++)
    {
        auto &no = nativeObjects[i];
        if (state == MS_none || no.state == state)
        {
            totalMemory += no.size;
            auto typeIndex = no.nativeTypeArrayIndex;
            auto iter = typeMemory.find(typeIndex);
            if (iter == typeMemory.end())
            {
                iter = typeMemory.insert(pair<int32_t, int32_t>(typeIndex, 0)).first;
                indice.push_back(typeIndex);
                
                typeCount.insert(pair<int32_t, int32_t>(typeIndex, 0));
            }
            
            iter->second += no.size;
            typeCount.at(typeIndex)++;
        }
    }
    
    std::sort(indice.begin(), indice.end(), [&](int32_t a, int32_t b)
              {
                  auto ma = typeMemory[a];
                  auto mb = typeMemory[b];
                  if (ma != mb) {return ma > mb;}
                  return typeCount.at(a) < typeCount.at(b);
              });
    
    vector<std::tuple<int32_t, double, double>> stats;
    
    double percent = 0;
    double accumulation = 0;
    for (auto i = 0; i < indice.size(); i++)
    {
        auto index = indice[i];
        percent = 100 * (typeMemory.at(index) / totalMemory);
        accumulation += percent;
        stats.push_back(std::make_tuple(index, percent, accumulation));
    }
    
    char progress[300+1];
    char fence[] = "█";
    auto &nativeTypes = *snapshot.nativeTypes;
    for (auto i = 0; i < stats.size(); i++)
    {
        if (rank > 0 && i >= rank){break;}
        auto &item = stats[i];
        auto typeIndex = std::get<0>(item);
        auto &type = nativeTypes[typeIndex];
        memset(progress, 0, sizeof(progress));
        auto count = std::max(1, (int32_t)std::round(std::get<1>(item)));
        printf("%6.2f %6.2f ", std::get<1>(item), std::get<2>(item));
        char *iter = progress;
        for (auto n = 0; n < count; n++)
        {
            memcpy(iter, fence, 3);
            iter += 3;
        }
        printf("%s %s %'d #%d *%d\n", progress, type.name->c_str(), typeMemory.at(typeIndex), typeCount.at(typeIndex), typeIndex);
    }
}

void MemorySnapshotCrawler::statHeap(int32_t rank)
{
    using std::get;
    
    auto totalMemory = 0;
    map<int32_t, int32_t> sizeMemory;
    map<int32_t, int32_t> sizeCount;
    
    vector<int32_t> indice;
    auto &sortedHeapSections = *snapshot.sortedHeapSections;
    for (auto iter = sortedHeapSections.begin(); iter != sortedHeapSections.end(); iter++)
    {
        auto &heap = **iter;
        auto match = sizeMemory.find(heap.size);
        if (match == sizeMemory.end())
        {
            match = sizeMemory.insert(pair<int32_t, int32_t>(heap.size, 0)).first;
            indice.push_back(heap.size);
            sizeCount.insert(pair<int32_t, int32_t>(heap.size, 0));
        }
        match->second += heap.size;
        sizeCount.at(heap.size)++;
        totalMemory += heap.size;
    }
    
    std::sort(indice.begin(), indice.end(), [&](int32_t a, int32_t b)
              {
                  auto ma = sizeMemory.at(a);
                  auto mb = sizeMemory.at(b);
                  if (ma != mb) {return ma > mb;}
                  return a > b;
              });
    
    double accumulation = 0;
    vector<std::tuple<int32_t, double, double, int32_t>> stats;
    for (auto i = indice.begin(); i != indice.end(); i++)
    {
        auto size = *i;
        auto rm = sizeMemory.at(size);
        double percent = 100 * (double)rm / totalMemory;
        accumulation += percent;
        stats.push_back(std::make_tuple(size, percent, accumulation, rm));
    }
    
    char percentage[300+1];
    char fence[] = "█";
    printf("[SUMMARY] blocks=%d memory=%d\n", (int32_t)sortedHeapSections.size(), totalMemory);
    for (auto i = 0; i < stats.size(); i++)
    {
        if (rank > 0 && i >= rank){break;}
        auto &item = stats[i];
        auto rank = get<0>(item);
        memset(percentage, 0, sizeof(percentage));
        auto count = std::max(1, (int32_t)std::round(get<1>(item)) * 2);
        char *iter = percentage;
        for (auto n = 0; n < count; n++)
        {
            memcpy(iter, fence, 3);
            iter += 3;
        }
        
        auto rm = get<3>(item);
        printf("%6.2f %6.2f %8d %5.0fK %s %d %.0fK #%d\n", get<1>(item), get<2>(item), rank, rank/1024.0, percentage, rm, rm/1024.0, sizeCount.at(rank));
    }
}

const char16_t *MemorySnapshotCrawler::getString(address_t address, int32_t &size)
{
    return __memoryReader->readString(address + __vm->objectHeaderSize, size);
}

const string MemorySnapshotCrawler::getUTFString(address_t address, int32_t &size, bool compactMode)
{
    auto *data = getString(address, size);
    if (data != nullptr)
    {
        auto utf = __convertor.to_bytes(data);
        if (compactMode)
        {
            for (auto iter = utf.begin(); iter != utf.end(); iter++)
            {
                if (*iter == '\n' || *iter == '\r')
                {
                    *iter = '\\'; // 禁止换行
                }
            }
        }
        return utf;
    }
    return string();
}

bool MemorySnapshotCrawler::isSubclassOfMType(TypeDescription &type, int32_t baseTypeIndex)
{
    if (type.typeIndex == baseTypeIndex) { return true; }
    if (type.typeIndex < 0 || baseTypeIndex < 0) { return false; }
    
    TypeDescription *iter = &type;
    while (iter->baseOrElementTypeIndex != -1)
    {
        iter = &snapshot.typeDescriptions->items[iter->baseOrElementTypeIndex];
        if (iter->typeIndex == baseTypeIndex) { return true; }
    }
    
    return false;
}

bool MemorySnapshotCrawler::isSubclassOfNType(PackedNativeType &type, int32_t baseTypeIndex)
{
    if (type.typeIndex == baseTypeIndex) { return true; }
    if (type.typeIndex < 0 || baseTypeIndex < 0) { return false; }
    
    PackedNativeType *iter = &type;
    while (iter->nativeBaseTypeArrayIndex != -1)
    {
        iter = &snapshot.nativeTypes->items[iter->nativeBaseTypeArrayIndex];
        if (iter->typeIndex == baseTypeIndex) { return true; }
    }
    
    return false;
}

bool MemorySnapshotCrawler::isPremitiveType(int32_t typeIndex)
{
    auto &mtypes = snapshot.managedTypeIndex;
    
    if (typeIndex == mtypes.system_IntPtr) {return true;}
    
    if (typeIndex == mtypes.system_Int64) {return true;}
    if (typeIndex == mtypes.system_Int32) {return true;}
    if (typeIndex == mtypes.system_Int16) {return true;}
    if (typeIndex == mtypes.system_SByte) {return true;}
    
    if (typeIndex == mtypes.system_UInt64) {return true;}
    if (typeIndex == mtypes.system_UInt32) {return true;}
    if (typeIndex == mtypes.system_UInt16) {return true;}
    if (typeIndex == mtypes.system_Byte) {return true;}
    if (typeIndex == mtypes.system_Char) {return true;}
    
    if (typeIndex == mtypes.system_Single) {return true;}
    if (typeIndex == mtypes.system_Double) {return true;}
    
    if (typeIndex == mtypes.system_Boolean) {return true;}
    return false;
}

void MemorySnapshotCrawler::dumpPremitiveValue(address_t address, int32_t typeIndex)
{
    address += __vm->objectHeaderSize;
    auto &mtypes = snapshot.managedTypeIndex;
    auto &memoryReader = *__memoryReader;
    
    if (typeIndex == mtypes.system_IntPtr) {printf("0x%08llx", memoryReader.readPointer(address));}
    
    if (typeIndex == mtypes.system_Int64) {printf("%lld", memoryReader.readInt64(address));}
    if (typeIndex == mtypes.system_Int32) {printf("%d", memoryReader.readInt32(address));}
    if (typeIndex == mtypes.system_Int16) {printf("%d", memoryReader.readInt16(address));}
    if (typeIndex == mtypes.system_SByte) {printf("%d", memoryReader.readInt8(address));}
    
    if (typeIndex == mtypes.system_UInt64) {printf("%lld", memoryReader.readUInt64(address));}
    if (typeIndex == mtypes.system_UInt32) {printf("%d", memoryReader.readUInt32(address));}
    if (typeIndex == mtypes.system_UInt16) {printf("%d", memoryReader.readUInt16(address));}
    if (typeIndex == mtypes.system_Byte) {printf("%d", memoryReader.readUInt8(address));}

    if (typeIndex == mtypes.system_Char) {printf("\\u%04x", memoryReader.readUInt16(address));}

    if (typeIndex == mtypes.system_Single) {printf("%f", memoryReader.readFloat(address));}
    if (typeIndex == mtypes.system_Double) {printf("%f", memoryReader.readDouble(address));}

    if (typeIndex == mtypes.system_Boolean) {printf("%s", memoryReader.readBoolean(address) ? "true" : "false");}
}

void MemorySnapshotCrawler::dumpMRefChain(address_t address, bool includeCircular, int32_t limit)
{
    auto objectIndex = findMObjectAtAddress(address);
    if (objectIndex == -1) {return;}
    
    auto *mo = &managedObjects[objectIndex];
    auto fullChains = iterateMRefChain(mo, vector<int32_t>(), set<int64_t>(), limit);
    for (auto c = fullChains.begin(); c != fullChains.end(); c++)
    {
        auto &chain = *c;
        auto number = 0;
        for (auto n = chain.rbegin(); n != chain.rend(); n++)
        {
            auto index = *n;
            if (index < 0)
            {
                if (!includeCircular) {break;}
                switch (index)
                {
                    case -1:
                        printf("∞"); // circular
                        continue;
                    case -2:
                        printf("*"); // more and interrupted
                        continue;
                    case -3:
                        printf("~"); // ignore in tracking mode
                        continue;
                }
            }
            
            auto &ec = connections[index];
            auto &node = managedObjects[ec.to];
            auto &joint = joints[ec.jointArrayIndex];
            auto &type = snapshot.typeDescriptions->items[node.typeIndex];
            
            if (number == 0)
            {
                if (ec.fromKind == CK_gcHandle)
                {
                    printf("<GCHandle>::%s 0x%08llx\n", type.name->c_str(), node.address);
                }
                else
                {
                    auto &hookType = snapshot.typeDescriptions->items[joint.hookTypeIndex];
                    auto &field = hookType.fields->items[joint.fieldSlotIndex];
                    if (ec.fromKind == CK_static)
                    {
                        auto &hookType = snapshot.typeDescriptions->items[joint.hookTypeIndex];
                        printf("<Static>::%s::", hookType.name->c_str());
                    }
                    
                    printf("{%s:%s} 0x%08llx\n", field.name->c_str(), type.name->c_str(), node.address);
                }
            }
            else
            {
                auto &hookType = snapshot.typeDescriptions->items[joint.hookTypeIndex];
                auto &field = hookType.fields->items[joint.fieldSlotIndex];
                if (joint.elementArrayIndex >= 0)
                {
                    printf("    .{%s:%s}[%d] 0x%08llx\n", field.name->c_str(), type.name->c_str(), joint.elementArrayIndex, node.address);
                }
                else
                {
                    printf("    .{%s:%s} 0x%08llx\n", field.name->c_str(), type.name->c_str(), node.address);
                }
            }
            
            ++number;
        }
    }
}

vector<vector<int32_t>> MemorySnapshotCrawler::iterateMRefChain(ManagedObject *mo,
                                                                vector<int32_t> chain, set<int64_t> antiCircular, int32_t limit, int64_t __iter_capacity, int32_t __depth)
{
    vector<vector<int32_t>> result;
    if (mo->fromConnections.size() > 0)
    {
        set<int64_t> unique;
        for (auto i = 0; i < mo->fromConnections.size(); i++)
        {
            if (limit > 0 && unique.size() >= limit) {break;}
            auto ci = mo->fromConnections[i];
            auto &ec = connections[ci];
            auto fromIndex = ec.from;
            
            vector<int32_t> __chain(chain);
            __chain.push_back(ci);
            
            if (ec.fromKind == CK_static || ec.fromKind == CK_gcHandle || fromIndex < 0)
            {
                result.push_back(__chain);
                continue;
            }
            
            int64_t uuid = ((int64_t)ec.fromKind << 30 | (int64_t)ec.from) << 32 | ((int64_t)ec.toKind << 30 | ec.to);
            if (unique.find(uuid) != unique.end()) {continue;}
            unique.insert(uuid);
            
            if (antiCircular.find(uuid) == antiCircular.end())
            {
                set<int64_t> __antiCircular(antiCircular);
                __antiCircular.insert(uuid);
                
                auto *fromObject = &managedObjects[fromIndex];
                auto depthCapacity = fromObject->fromConnections.size();
                if ((__iter_capacity * depthCapacity >= REF_ITERATE_CAPACITY && limit <= 0) || __depth >= REF_ITERATE_DEPTH)
                {
                    __chain.push_back(-2); // interruptted signal
                    return {__chain};
                }
                
                auto branches = iterateMRefChain(fromObject, __chain, __antiCircular, limit, __iter_capacity * depthCapacity, __depth + 1);
                if (branches.size() != 0)
                {
                    result.insert(result.end(), branches.begin(), branches.end());
                }
            }
            else
            {
                __chain.push_back(-1); // circular signal
                result.push_back(__chain); // circular reference situation
            }
        }
    }
    else
    {
        if (chain.size() != 0){result.push_back(chain);}
    }
    
    return result;
}

void MemorySnapshotCrawler::dumpNRefChain(address_t address, bool includeCircular, int32_t limit)
{
    auto objectIndex = findNObjectAtAddress(address);
    if (objectIndex == -1) {return;}
    
    auto *no = &snapshot.nativeObjects->items[objectIndex];
    auto &nativeConnections = *snapshot.connections;
    auto fullChains = iterateNRefChain(no, vector<int32_t>(), set<int64_t>(), limit);
    for (auto c = fullChains.begin(); c != fullChains.end(); c++)
    {
        auto &chain = *c;
        auto number = 0;
        for (auto n = chain.rbegin(); n != chain.rend(); n++)
        {
            auto index = *n;
            if (index < 0)
            {
                if (!includeCircular) {break;}
                switch (index)
                {
                    case -1:
                        printf("∞"); // circular
                        continue;
                    case -2:
                        printf("*"); // more and interrupted
                        continue;
                    case -3:
                        printf("~"); // ignore in tracking mode
                        continue;
                }
            }
            
            auto &nc = nativeConnections[index];
            if (number == 0)
            {
                if (nc.fromKind == CK_gcHandle)
                {
                    assert(number == 0);
                    printf("<gcHandle>.");
                }
                else
                {
                    auto &node = snapshot.nativeObjects->items[nc.from];
                    auto &type = snapshot.nativeTypes->items[node.nativeTypeArrayIndex];
                    if (node.isDontDestroyOnLoad)
                    {
                        printf("<DDO>."); // Don't Destroy Object
                    }
                    else if (node.isManager)
                    {
                        printf("<UMO>."); // Unity Managed Object
                    }
                    else if (node.isPersistent)
                    {
                        printf("<SIS>."); // Store In Scene
                    }
                    else
                    {
                        printf("<%d>.", node.instanceId);
                    }
                    printf("{%s:0x%08llx:'%s'}.", type.name->c_str(), node.nativeObjectAddress, node.name->c_str());
                }
            }
            auto &node = snapshot.nativeObjects->items[nc.to];
            auto &type = snapshot.nativeTypes->items[node.nativeTypeArrayIndex];
            printf("{%s:0x%08llx:'%s'}.", type.name->c_str(), node.nativeObjectAddress, node.name->c_str());
            ++number;
        }
        if (number != 0){printf("\b \n");}
    }
}

vector<vector<int32_t>> MemorySnapshotCrawler::iterateNRefChain(PackedNativeUnityEngineObject *no,
                                                                vector<int32_t> chain, set<int64_t> antiCircular, int32_t limit, int64_t __iter_capacity, int32_t __depth)
{
    vector<vector<int32_t>> result;
    if (no->fromConnections.size() > 0)
    {
        set<int64_t> unique;
        for (auto i = 0; i < no->fromConnections.size(); i++)
        {
            if (limit > 0 && unique.size() >= limit) {break;}
            auto ci = no->fromConnections[i];
            auto &nc = snapshot.connections->items[ci];
            auto fromIndex = nc.from;
            
            vector<int32_t> __chain(chain);
            __chain.push_back(ci);
            
            if (nc.fromKind != CK_native)
            {
                result.push_back(__chain);
                continue;
            }
            
            int64_t uuid = (int64_t)nc.from << 32 | nc.to;
            if (unique.find(uuid) != unique.end()){continue;}
            unique.insert(uuid);
            
            if (antiCircular.find(uuid) == antiCircular.end())
            {
                set<int64_t> __antiCircular(antiCircular);
                __antiCircular.insert(uuid);
                
                auto *fromObject = &snapshot.nativeObjects->items[fromIndex];
                auto depthCapacity = fromObject->fromConnections.size();
                if ((__iter_capacity * depthCapacity >= REF_ITERATE_CAPACITY && limit <= 0) || __depth >= REF_ITERATE_DEPTH)
                {
                    __chain.push_back(-2); // interruptted signal
                    result.push_back(__chain);
                    continue;
                }
                
                auto branches = iterateNRefChain(fromObject, __chain, __antiCircular, limit, __iter_capacity * depthCapacity, __depth + 1);
                if (branches.size() != 0)
                {
                    result.insert(result.end(), branches.begin(), branches.end());
                }
            }
            else
            {
                __chain.push_back(-1); // circular signal
                result.push_back(__chain); // circular reference situation
            }
        }
    }
    else
    {
        if (chain.size() != 0){result.push_back(chain);}
    }
    
    return result;
}

void MemorySnapshotCrawler::tryAcceptConnection(Connection &nc)
{
    if (nc.fromKind == CK_native)
    {
        auto &no = snapshot.nativeObjects->items[nc.from];
        no.toConnections.push_back(nc.connectionArrayIndex);
    }
    
    if (nc.toKind == CK_native)
    {
        auto &no = snapshot.nativeObjects->items[nc.to];
        no.fromConnections.push_back(nc.connectionArrayIndex);
    }
}

void MemorySnapshotCrawler::tryAcceptConnection(EntityConnection &ec)
{
    if (ec.fromKind != CK_none && ec.from >= 0)
    {
        auto &mo = managedObjects[ec.from];
        mo.toConnections.push_back(ec.connectionArrayIndex);
    }
    
    if (ec.toKind != CK_none && ec.to >= 0)
    {
        auto &mo = managedObjects[ec.to];
        mo.fromConnections.push_back(ec.connectionArrayIndex);
    }
}

int32_t MemorySnapshotCrawler::findTypeAtTypeAddress(address_t address)
{
    if (address == 0) {return -1;}
    if (__typeAddressMap.size() == 0)
    {
        Array<TypeDescription> &typeDescriptions = *snapshot.typeDescriptions;
        for (auto i = 0; i < typeDescriptions.size; i++)
        {
            auto &type = typeDescriptions[i];
            __typeAddressMap.insert(pair<address_t, int32_t>(type.typeInfoAddress, type.typeIndex));
        }
    }
    
    auto iter = __typeAddressMap.find(address);
    return iter != __typeAddressMap.end() ? iter->second : -1;
}

int32_t MemorySnapshotCrawler::findTypeOfAddress(address_t address)
{
    auto typeIndex = findTypeAtTypeAddress(address);
    if (typeIndex != -1) {return typeIndex;}
    auto typePtr = __memoryReader->readPointer(address);
    if (typePtr == 0) {return -1;}
    auto vtablePtr = __memoryReader->readPointer(typePtr);
    if (vtablePtr != 0)
    {
        return findTypeAtTypeAddress(vtablePtr);
    }
    return findTypeAtTypeAddress(typePtr);
}

void MemorySnapshotCrawler::dumpRedundants(int32_t typeIndex)
{
    auto &type = snapshot.typeDescriptions->items[typeIndex];
    
    map<size_t, vector<int32_t>> stats;
    for (auto i = 0; i < managedObjects.size(); i++)
    {
        auto &mo = managedObjects[i];
        if (mo.typeIndex == typeIndex)
        {
            auto *data = __memoryReader->readMemory(mo.address);
            
            size_t hash = 0;
            if (type.isValueType)
            {
                 hash = __hash.get(data + __vm->objectHeaderSize, mo.size);
            }
            else
            {
                hash = __hash.get(data, mo.size);
            }
            
            auto match = stats.find(hash);
            if (match == stats.end())
            {
                match = stats.insert(pair<size_t, vector<int32_t>>(hash, vector<int32_t>())).first;
            }
            
            match->second.push_back(mo.managedObjectIndex);
        }
    }
    
    map<size_t, int32_t> memory;
    vector<size_t> target;
    
    printf("%s typeIndex=%d instanceCount=%d instanceMemory=%d\n", type.name->c_str(), typeIndex, type.instanceCount, type.instanceMemory);
    for (auto iter = stats.begin(); iter != stats.end(); iter++)
    {
        auto &children = iter->second;
        if (children.size() == 1) {continue;}
        
        auto total = 0;
        for (auto n = children.begin(); n != children.end(); n++)
        {
            auto &mo = managedObjects[*n];
            total += mo.size;
        }
        
        memory.insert(pair<size_t, int32_t>(iter->first, total));
        target.push_back(iter->first);
    }
    
    std::sort(target.begin(), target.end(), [&](size_t a, size_t b)
              {
                  auto ma = memory.at(a);
                  auto mb = memory.at(b);
                  if (ma != mb) { return ma > mb; }
                  return a > b;
              });
    auto isString = type.typeIndex == snapshot.managedTypeIndex.system_String;
    for (auto iter = target.begin(); iter != target.end(); iter++)
    {
        auto hash = *iter;
        auto &children = stats.at(hash);
        printf("\e[36m%8d #%-2d", memory.at(hash), (int32_t)children.size());
        
        bool extraComplate = false;
        for (auto n= children.begin(); n != children.end(); n++)
        {
            auto &mo = managedObjects[*n];
            auto size = 0;
            if (!extraComplate && isString) {printf(" \e[32m'%s'", getUTFString(mo.address, size, true).c_str());}
            printf(" \e[33m0x%08llx", mo.address);
            if (type.isArray) {printf(":%d", mo.size);}
            extraComplate = true;
        }
        printf("\n");
    }
}

address_t MemorySnapshotCrawler::findMObjectOfNObject(address_t address)
{
    if (address == 0){return -1;}
    if (__managedNativeAddressMap.size() == 0)
    {
        for (auto i = 0; i < managedObjects.size(); i++)
        {
            auto &mo = managedObjects[i];
            if (mo.nativeObjectIndex >= 0)
            {
                auto &no = snapshot.nativeObjects->items[mo.nativeObjectIndex];
                __managedNativeAddressMap.insert(pair<address_t, int32_t>(no.nativeObjectAddress, mo.managedObjectIndex));
            }
        }
    }
    
    auto iter = __managedNativeAddressMap.find(address);
    if (iter == __managedNativeAddressMap.end())
    {
        return 0;
    }
    else
    {
        return managedObjects[iter->second].address;
    }
}

address_t MemorySnapshotCrawler::findNObjectOfMObject(address_t address)
{
    if (address == 0){return -1;}
    if (__nativeManagedAddressMap.size() == 0)
    {
        for (auto i = 0; i < managedObjects.size(); i++)
        {
            auto &mo = managedObjects[i];
            if (mo.nativeObjectIndex >= 0)
            {
                __nativeManagedAddressMap.insert(pair<address_t, int32_t>(mo.address, mo.nativeObjectIndex));
            }
        }
    }
    
    auto iter = __nativeManagedAddressMap.find(address);
    if (iter == __nativeManagedAddressMap.end())
    {
        return 0;
    }
    else
    {
        return snapshot.nativeObjects->items[iter->second].nativeObjectAddress;
    }
}

int32_t MemorySnapshotCrawler::findMObjectAtAddress(address_t address)
{
    if (address == 0){return -1;}
    if (__managedObjectAddressMap.size() == 0)
    {
        for (auto i = 0; i < managedObjects.size(); i++)
        {
            auto &mo = managedObjects[i];
            __managedObjectAddressMap.insert(pair<address_t, int32_t>(mo.address, mo.managedObjectIndex));
        }
    }
    
    auto iter = __managedObjectAddressMap.find(address);
    return iter != __managedObjectAddressMap.end() ? iter->second : -1;
}

int32_t MemorySnapshotCrawler::findNObjectAtAddress(address_t address)
{
    if (address == 0){return -1;}
    if (__nativeObjectAddressMap.size() == 0)
    {
        auto &nativeObjects = *snapshot.nativeObjects;
        for (auto i = 0; i < nativeObjects.size; i++)
        {
            auto &no = nativeObjects[i];
            __nativeObjectAddressMap.insert(pair<address_t, int32_t>(no.nativeObjectAddress, no.nativeObjectArrayIndex));
        }
    }
    
    auto iter = __nativeObjectAddressMap.find(address);
    return iter != __nativeObjectAddressMap.end() ? iter->second : -1;
}

void MemorySnapshotCrawler::tryConnectWithNativeObject(ManagedObject &mo)
{
    if (mo.nativeObjectIndex >= 0){return;}
    
    auto &type = snapshot.typeDescriptions->items[mo.typeIndex];
    mo.isValueType = type.isValueType;
    if (mo.isValueType || type.isArray || !type.isUnityEngineObjectType) {return;}
    
    auto nativeAddress = __memoryReader->readPointer(mo.address + snapshot.cached_ptr->offset);
    if (nativeAddress == 0){return;}
    
    auto nativeObjectIndex = findNObjectAtAddress(nativeAddress);
    if (nativeObjectIndex == -1){return;}
    
    // connect managed/native objects
    auto &no = snapshot.nativeObjects->items[nativeObjectIndex];
    mo.nativeObjectIndex = nativeObjectIndex;
    mo.nativeSize = no.size;
    no.managedObjectArrayIndex = mo.managedObjectIndex;
    
    // connect managed/native types
    auto &nativeType = snapshot.nativeTypes->items[no.nativeTypeArrayIndex];
    type.nativeTypeArrayIndex = nativeType.typeIndex;
    nativeType.managedTypeArrayIndex = type.typeIndex;
}

ManagedObject &MemorySnapshotCrawler::createManagedObject(address_t address, int32_t typeIndex)
{
    auto &mo = managedObjects.add();
    mo.address = address;
    mo.typeIndex = typeIndex;
    mo.managedObjectIndex = managedObjects.size() - 1;
    tryConnectWithNativeObject(mo);
#if USE_ADDRESS_MIRROR
    assert(address == __mirror[mo.managedObjectIndex]);
#endif
    return mo;
}

bool MemorySnapshotCrawler::isCrawlable(TypeDescription &type)
{
    return !type.isValueType || type.size > 8; // vm->pointerSize
}

bool MemorySnapshotCrawler::crawlManagedArrayAddress(address_t address, TypeDescription &type, HeapMemoryReader &memoryReader, EntityJoint &joint, int32_t depth)
{
    auto isStaticCrawling = memoryReader.isStatic();
    if (address < 0 || (!isStaticCrawling && address == 0)){return false;}
    
    auto &elementType = snapshot.typeDescriptions->items[type.baseOrElementTypeIndex];
    if (!isCrawlable(elementType)) {return false;}
    
    auto successCount = 0;
    address_t elementAddress = 0;
    auto elementCount = memoryReader.readArrayLength(address, type);
    for (auto i = 0; i < elementCount; i++)
    {
        if (elementType.isValueType)
        {
            elementAddress = address + __vm->arrayHeaderSize + i *elementType.size - __vm->objectHeaderSize;
        }
        else
        {
            auto ptrAddress = address + __vm->arrayHeaderSize + i * __vm->pointerSize;
            elementAddress = memoryReader.readPointer(ptrAddress);
        }
        
        auto &elementJoint = joints.clone(joint);
        elementJoint.jointArrayIndex = joints.size() - 1;
        elementJoint.isConnected = false;
        
        // set element info
        elementJoint.elementArrayIndex = i;
        
        auto success = crawlManagedEntryAddress(elementAddress, &elementType, memoryReader, elementJoint, false, depth + 1);
        if (success) { successCount += 1; }
    }
    
    return successCount != 0;
}

bool MemorySnapshotCrawler::crawlManagedEntryAddress(address_t address, TypeDescription *type, HeapMemoryReader &memoryReader, EntityJoint &joint, bool isActualType, int32_t depth)
{
    auto isStaticCrawling = memoryReader.isStatic();
    if (address < 0 || (!isStaticCrawling && address == 0)){return false;}
    if (depth >= 512) {return false;}
    
    int32_t typeIndex = -1;
    if (type != nullptr && type->isValueType)
    {
        typeIndex = type->typeIndex;
    }
    else
    {
        if (isActualType)
        {
            typeIndex = type->typeIndex;
        }
        else
        {
            typeIndex = findTypeOfAddress(address);
            if (typeIndex == -1 && type != nullptr) {typeIndex = type->typeIndex;}
        }
    }
    if (typeIndex == -1){return false;}
    
    auto &entryType = snapshot.typeDescriptions->items[typeIndex];
    
    ManagedObject *mo;
    auto iter = __crawlingVisit.find(address);
    if (entryType.isValueType || iter == __crawlingVisit.end())
    {
        mo = &createManagedObject(address, typeIndex);
        mo->size = memoryReader.readObjectSize(mo->address, entryType);
        mo->isValueType = entryType.isValueType;
    }
    else
    {
        auto managedObjectIndex = iter->second;
        mo = &managedObjects[managedObjectIndex];
    }
    
    assert(mo->managedObjectIndex >= 0);
    
    auto &ec = connections.add();
    ec.connectionArrayIndex = connections.size() - 1;
    if (joint.gcHandleIndex >= 0)
    {
        ec.fromKind = CK_gcHandle;
        ec.from = joint.gcHandleIndex;
    }
    else if (joint.isStatic)
    {
        ec.fromKind = CK_static;
        ec.from = -1;
    }
    else
    {
        ec.fromKind = CK_managed;
        ec.from = joint.hookObjectIndex;
    }
    
    ec.toKind = CK_managed;
    ec.to = mo->managedObjectIndex;
    ec.jointArrayIndex = joint.jointArrayIndex;
    joint.isConnected = true;
    
    tryAcceptConnection(ec);
    
    if (!entryType.isValueType)
    {
        if (iter != __crawlingVisit.end()) {return false;}
        __crawlingVisit.insert(pair<address_t, int32_t>(address, mo->managedObjectIndex));
    }
    
    if (entryType.isArray)
    {
        return crawlManagedArrayAddress(address, entryType, memoryReader, joint, depth + 1);
    }
    
    auto successCount = 0;
    auto iterType = &entryType;
    while (iterType != nullptr)
    {
        for (auto i = 0; i < iterType->fields->size; i++)
        {
            auto &field = iterType->fields->items[i];
            if (field.isStatic){continue;}
            
            auto *fieldType = &snapshot.typeDescriptions->items[field.typeIndex];
            if (!isCrawlable(*fieldType)){continue;}
            
            address_t fieldAddress = 0;
            if (fieldType->isValueType)
            {
                fieldAddress = address + field.offset - __vm->objectHeaderSize;
            }
            else
            {
                address_t ptrAddress = 0;
                if (memoryReader.isStatic())
                {
                    ptrAddress = address + field.offset - __vm->objectHeaderSize;
                }
                else
                {
                    ptrAddress = address + field.offset;
                }
                fieldAddress = memoryReader.readPointer(ptrAddress);
                auto fieldTypeIndex = findTypeOfAddress(fieldAddress);
                if (fieldTypeIndex != -1)
                {
                    fieldType = &snapshot.typeDescriptions->items[fieldTypeIndex];
                }
            }
            
            auto *reader = &memoryReader;
            if (!fieldType->isValueType)
            {
                reader = this->__memoryReader;
            }
            
            auto &ej = joints.add();
            
            // set field hook info
            ej.jointArrayIndex = joints.size() - 1;
            ej.hookObjectAddress = address;
            ej.hookObjectIndex = mo->managedObjectIndex;
            ej.hookTypeIndex = iterType->typeIndex;
            
            // set field info
            ej.fieldAddress = fieldAddress;
            ej.fieldTypeIndex = field.typeIndex;
            ej.fieldSlotIndex = field.fieldSlotIndex;
            ej.fieldOffset = field.offset;
            
            auto success = crawlManagedEntryAddress(fieldAddress, fieldType, *reader, ej, true, depth + 1);
            if (success) { successCount += 1; }
        }
        
        if (iterType->baseOrElementTypeIndex == -1)
        {
            iterType = nullptr;
        }
        else
        {
            iterType = &snapshot.typeDescriptions->items[iterType->baseOrElementTypeIndex];
        }
    }
    
    return successCount != 0;
}

void MemorySnapshotCrawler::inspectMObject(address_t address)
{
    dumpMObjectHierarchy(address, nullptr, set<int64_t>(), false, "");
}

void MemorySnapshotCrawler::inspectNObject(address_t address)
{
    auto index = findNObjectAtAddress(address);
    if (index <= 0) {return;}
    
    dumpNObjectHierarchy(&snapshot.nativeObjects->items[index], set<int64_t>(), "");
}

void MemorySnapshotCrawler::inspectMType(int32_t typeIndex)
{
    if (typeIndex < 0 || typeIndex >= snapshot.typeDescriptions->size) {return;}
    
    auto &type = snapshot.typeDescriptions->items[typeIndex];
    printf("0x%08llx name='%s'%d size=%d", type.typeInfoAddress, type.name->c_str(), type.typeIndex, type.size);
    if (type.baseOrElementTypeIndex >= 0)
    {
        auto &baseType = snapshot.typeDescriptions->items[type.baseOrElementTypeIndex];
        printf(" baseOrElementType='%s'%d", baseType.name->c_str(), baseType.typeIndex);
    }
    if (type.isValueType) {printf(" isValueType=%s", type.isValueType ? "true" : "false");}
    if (type.isArray) {printf(" isArray=%s arrayRank=%d", type.isArray ? "true" : "false", type.arrayRank);}
    if (type.staticFieldBytes != nullptr) {printf(" staticFieldBytes=%d", type.staticFieldBytes->size);}
    printf(" assembly='%s' instanceMemory=%d instanceCount=%d", type.assembly->c_str(), type.instanceMemory, type.instanceCount);
    if (type.nativeTypeArrayIndex >= 0)
    {
        auto &nt = snapshot.nativeTypes->items[type.nativeTypeArrayIndex];
        printf(" NATIVE[instanceMemory=%d instanceCount=%d]", nt.instanceMemory, nt.instanceCount);
    }
    printf("\n");
    
    for (auto i = 0; i < type.fields->size; i++)
    {
        auto &field = type.fields->items[i];
        printf("    isStatic=%s name='%s' offset=%d typeIndex=%d\n", field.isStatic ? "true" : "false", field.name->c_str(), field.offset, field.typeIndex);
    }
}

void MemorySnapshotCrawler::inspectNType(int32_t typeIndex)
{
    if (typeIndex < 0 || typeIndex >= snapshot.nativeTypes->size) {return;}
    
    auto &type = snapshot.nativeTypes->items[typeIndex];
    printf("name='%s'%d", type.name->c_str(), type.typeIndex);
    if (type.nativeBaseTypeArrayIndex >= 0)
    {
        auto &baseType = snapshot.nativeTypes->items[type.nativeBaseTypeArrayIndex];
        printf(" nativeBaseType='%s'%d", baseType.name->c_str(), baseType.typeIndex);
    }
    printf(" instanceMemory=%d instanceCount=%d", type.instanceMemory, type.instanceCount);
    if (type.managedTypeArrayIndex >= 0)
    {
        auto &mt = snapshot.typeDescriptions->items[type.managedTypeArrayIndex];
        printf(" MANAGED[0x%08llx typeIndex=%d instanceMemory=%d instanceCount=%d]", mt.typeInfoAddress, mt.typeIndex, mt.instanceMemory, mt.instanceCount);
    }
    printf("\n");
}

void MemorySnapshotCrawler::findMObject(address_t address)
{
    auto index = findMObjectAtAddress(address);
    if (index >= 0)
    {
        auto &mo = managedObjects[index];
        auto &type = snapshot.typeDescriptions->items[mo.typeIndex];
        printf("0x%08llx type='%s'%d size=%d assembly='%s'", address, type.name->c_str(), type.typeIndex, mo.size, type.assembly->c_str());
        if (type.nativeTypeArrayIndex >= 0)
        {
            auto __address = findNObjectOfMObject(address);
            auto __index = findNObjectAtAddress(__address);
            assert(__index >= 0);
            
            auto &no = snapshot.nativeObjects->items[__index];
            printf(" NATIVE[0x%08llx size=%d]", no.nativeObjectAddress, no.size);
        }
        std::cout << std::endl;
    }
    else
    {
        printf("not found managed object at address[%08lldx]\n", address);
    }
}

void MemorySnapshotCrawler::findNObject(address_t address)
{
    auto index = findNObjectAtAddress(address);
    if (index >= 0)
    {
        auto &no = snapshot.nativeObjects->items[index];
        auto &nt = snapshot.nativeTypes->items[no.nativeTypeArrayIndex];
        printf("0x%08llx name='%s' type='%s'%d size=%d", no.nativeObjectAddress, no.name->c_str(), nt.name->c_str(), nt.typeIndex, no.size);
        if (no.managedObjectArrayIndex >= 0)
        {
            auto &mo = managedObjects[no.managedObjectArrayIndex];
            auto &mt = snapshot.typeDescriptions->items[mo.typeIndex];
            printf(" MANAGED[0x%08llx type='%s'%d size=%d]", mo.address, mt.name->c_str(), mt.typeIndex, mo.size);
        }
        printf("\n");
    }
    else
    {
        printf("not found native object at address[%08lldx]\n", address);
    }
}

void MemorySnapshotCrawler::dumpByteArray(const char *data, int32_t size)
{
    auto iter = data;
    char str[size * 2 + 1];
    memset(str, 0, sizeof(str));
    char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    for (auto i = 0; i < size; i++)
    {
        auto b = *iter;
        str[i * 2] = hex[b >> 4 & 0xF];
        str[i * 2 + 1] = hex[b & 0xF];
        iter++;
    }
    printf("%s", str);
}

void MemorySnapshotCrawler::dumpNObjectHierarchy(PackedNativeUnityEngineObject *no, set<int64_t> antiCircular, const char *indent, int32_t __iter_depth, int32_t __iter_capacity)
{
    auto __size = strlen(indent);
    char __indent[__size + 2*3 + 1]; // indent + 2×tabulator + \0
    memset(__indent, 0, sizeof(__indent));
    memcpy(__indent, indent, __size);
    char *tabular = __indent + __size;
    memcpy(tabular + 3, "─", 3);
    
    if (__iter_depth == 0)
    {
        auto &type = snapshot.nativeTypes->items[no->nativeTypeArrayIndex];
        printf("%s'%s':%s 0x%08llx\n", indent, no->name->c_str(), type.name->c_str(), no->nativeObjectAddress);
    }
    
    static const int32_t ITER_HIERARCHY_CAPACITY = 256;
    
    auto toCount = (int32_t)no->toConnections.size();
    if (toCount * __iter_capacity > ITER_HIERARCHY_CAPACITY)
    {
        toCount = ITER_HIERARCHY_CAPACITY / __iter_capacity;
    }
    
    for (auto i = 0; i < toCount; i++)
    {
        auto closed = i + 1 == toCount;
        auto &nc = snapshot.connections->items[no->toConnections[i]];
        auto &no = snapshot.nativeObjects->items[nc.to];
        auto &nt = snapshot.nativeTypes->items[no.nativeTypeArrayIndex];
        
        closed ? memcpy(tabular, "└", 3) : memcpy(tabular, "├", 3);
        printf("%s'%s':%s 0x%08llx=%d\n", __indent, no.name->c_str(), nt.name->c_str(), no.nativeObjectAddress, no.size);
        
        if (antiCircular.find(no.nativeObjectAddress) == antiCircular.end())
        {
            decltype(antiCircular) __antiCircular(antiCircular);
            __antiCircular.insert(no.nativeObjectAddress);
            
            if (closed)
            {
                char __nest_indent[__size + 1 + 2 + 1]; // indent + space + 2×space + \0
                memcpy(__nest_indent, indent, __size);
                memset(__nest_indent + __size, '\x20', 3);
                memset(__nest_indent + __size + 3, 0, 1);
                dumpNObjectHierarchy(&no, __antiCircular, __nest_indent, __iter_depth + 1, __iter_capacity * toCount);
            }
            else
            {
                char __nest_indent[__size + 3 + 2 + 1]; // indent + tabulator + 2×space + \0
                char *iter = __nest_indent + __size;
                memcpy(__nest_indent, indent, __size);
                memcpy(iter, "│", 3);
                memset(iter + 3, '\x20', 2);
                memset(iter + 5, 0, 1);
                dumpNObjectHierarchy(&no, __antiCircular, __nest_indent, __iter_depth + 1, __iter_capacity * toCount);
            }
        }
    }
}

void MemorySnapshotCrawler::dumpMObjectHierarchy(address_t address, TypeDescription *type,
                                                 set<int64_t> antiCircular, bool isActualType, const char *indent, int32_t __iter_depth)
{
    auto __size = strlen(indent);
    char __indent[__size + 2*3 + 1]; // indent + 2×tabulator + \0
    memset(__indent, 0, sizeof(__indent));
    memcpy(__indent, indent, __size);
    char *tabular = __indent + __size;
    memcpy(tabular + 3, "─", 3);
    
    if (type == nullptr || (!type->isValueType && !isActualType))
    {
        auto typeIndex = findTypeOfAddress(address);
        if (typeIndex == -1){return;}
        
        type = &snapshot.typeDescriptions->items[typeIndex];
    }
    
    auto &memoryReader = *__memoryReader;
    
    auto &entryType = *type;
    if (__iter_depth == 0)
    {
        printf("%s 0x%08llx\n", entryType.name->c_str(), address);
    }
    
    if (entryType.isArray)
    {
        auto *elementType = &snapshot.typeDescriptions->items[entryType.baseOrElementTypeIndex];
        auto __is_premitive = isPremitiveType(elementType->typeIndex);
        auto __is_string = elementType->typeIndex == snapshot.managedTypeIndex.system_String;
        
        address_t elementAddress = 0;
        auto elementCount = memoryReader.readArrayLength(address, entryType);
        if (elementType->typeIndex == snapshot.managedTypeIndex.system_Byte)
        {
            printf("%s└<%d>", __indent, elementCount);
            auto ptr = memoryReader.readMemory(address + __vm->arrayHeaderSize);
            dumpByteArray(ptr, elementCount);
            printf("\n");
            return;
        }
        
        for (auto i = 0; i < elementCount; i++)
        {
            bool closed = i + 1 == elementCount;
            decltype(antiCircular) __antiCircular(antiCircular);
            if (elementType->isValueType)
            {
                elementAddress = address + __vm->arrayHeaderSize + i *elementType->size - __vm->objectHeaderSize;
            }
            else
            {
                auto ptrAddress = address + __vm->arrayHeaderSize + i * __vm->pointerSize;
                elementAddress = memoryReader.readPointer(ptrAddress);
                auto typeIndex = findTypeOfAddress(elementAddress);
                if (typeIndex >= 0)
                {
                    elementType = &snapshot.typeDescriptions->items[typeIndex];
                }
                
                __antiCircular.insert(elementAddress);
            }
            
            closed ? memcpy(tabular, "└", 3) : memcpy(tabular, "├", 3);
            printf("%s[%d]:%s", __indent, i, elementType->name->c_str());
            if (__is_premitive)
            {
                printf(" = ");
                dumpPremitiveValue(elementAddress, elementType->typeIndex);
                printf("\n");
                continue;
            }
            
            if (__is_string)
            {
                auto size = 0;
                printf(" 0x%08llx = '%s'\n", elementAddress, getUTFString(elementAddress, size, true).c_str());
                continue;
            }
            
            if (elementAddress == 0)
            {
                printf(" = NULL\n");
                continue;
            }
            
            if (!elementType->isValueType) {printf(" 0x%08llx", elementAddress);}
            printf("\n");
            if (closed)
            {
                char __nest_indent[__size + 1 + 2 + 1]; // indent + space + 2×space + \0
                memcpy(__nest_indent, indent, __size);
                memset(__nest_indent + __size, '\x20', 3);
                memset(__nest_indent + __size + 3, 0, 1);
                dumpMObjectHierarchy(elementAddress, elementType, __antiCircular, true, __nest_indent, __iter_depth + 1);
            }
            else
            {
                char __nest_indent[__size + 3 + 2 + 1]; // indent + tabulator + 2×space + \0
                char *iter = __nest_indent + __size;
                memcpy(__nest_indent, indent, __size);
                memcpy(iter, "│", 3);
                memset(iter + 3, '\x20', 2);
                memset(iter + 5, 0, 1);
                dumpMObjectHierarchy(elementAddress, elementType, __antiCircular, true, __nest_indent, __iter_depth + 1);
            }
        }
        
        return;
    }
    
    auto *iterType = &entryType;
    vector<FieldDescription *> typeMembers;
    while (iterType != nullptr)
    {
        for (auto i = 0; i < iterType->fields->size; i++)
        {
            auto &field = iterType->fields->items[i];
            if (!field.isStatic) { typeMembers.push_back(&field); }
        }
        
        if (iterType->baseOrElementTypeIndex >= 0)
        {
            iterType = &snapshot.typeDescriptions->items[iterType->baseOrElementTypeIndex];
        }
        else
        {
            iterType = nullptr;
        }
    }
    
    auto fieldCount = typeMembers.size();
    for (auto i = 0; i < fieldCount; i++)
    {
        auto code = 0;
        auto closed = i + 1 == fieldCount;
        auto &field = *typeMembers[i];
        auto __is_premitive = isPremitiveType(field.typeIndex);
        
        address_t fieldAddress = 0;
        auto *fieldType = &snapshot.typeDescriptions->items[field.typeIndex];
        if (!fieldType->isValueType)
        {
            fieldAddress = memoryReader.readPointer(address + field.offset);
            if (field.typeIndex == snapshot.managedTypeIndex.system_String)
            {
                code = 2;
            }
            
            if (fieldAddress == 0)
            {
                code = 1;
            }
            
            auto typeIndex = findTypeOfAddress(fieldAddress);
            if (typeIndex >= 0)
            {
                fieldType = &snapshot.typeDescriptions->items[typeIndex];
            }
        }
        else
        {
            fieldAddress = address + field.offset - __vm->objectHeaderSize;
            if (field.typeIndex == entryType.typeIndex || __is_premitive)
            {
                code = 3;
            }
        }
        
        closed ? memcpy(tabular, "└", 3) : memcpy(tabular, "├", 3);
        const char *fieldTypeName = fieldType->name->c_str();
        const char *fieldName = field.name->c_str();
        switch (code)
        {
            case 1: // null
                printf("%s%s:%s = NULL", __indent, fieldName, fieldTypeName);
                break;
            case 2: // string
            {
                auto size = 0;
                printf("%s%s:%s 0x%08llx = '%s'", __indent, fieldName, fieldTypeName, fieldAddress , getUTFString(fieldAddress, size, true).c_str());
                break;
            }
            case 3: // premitive
                printf("%s%s:%s = ", __indent, fieldName, fieldTypeName);
                dumpPremitiveValue(fieldAddress, fieldType->typeIndex);
                break;
            default:
                printf("%s%s:%s", __indent, fieldName, fieldTypeName);
                if (!fieldType->isValueType)
                {
                    printf(" 0x%08llx", fieldAddress);
                }
                break;
        }
        printf("\n");
        
        if ((fieldType->isValueType && !__is_premitive) || ((antiCircular.find(fieldAddress) == antiCircular.end() && code == 0)))
        {
            decltype(antiCircular) __antiCircular(antiCircular);
            __antiCircular.insert(fieldAddress);
            
            if (closed)
            {
                char __nest_indent[__size + 1 + 2 + 1]; // indent + space + 2×space + \0
                memcpy(__nest_indent, __indent, __size);
                memset(__nest_indent + __size, '\x20', 3);
                memset(__nest_indent + __size + 3, 0, 1);
                dumpMObjectHierarchy(fieldAddress, fieldType, __antiCircular, true, __nest_indent, __iter_depth + 1);
            }
            else
            {
                char __nest_indent[__size + 3 + 2 + 1]; // indent + tabulator + 2×space + \0
                char *iter = __nest_indent + __size;
                memcpy(__nest_indent, __indent, __size);
                memcpy(iter, "│", 3);
                memset(iter + 3, '\x20', 2);
                memset(iter + 5, 0, 1);
                dumpMObjectHierarchy(fieldAddress, fieldType, __antiCircular, true, __nest_indent, __iter_depth + 1);
            }
        }
    }
}

void MemorySnapshotCrawler::crawlGCHandles()
{
    __sampler.begin("crawlGCHandles");
    auto &gcHandles = *snapshot.gcHandles;
    for (auto i = 0; i < gcHandles.size; i++)
    {
        auto &item = gcHandles[i];
        
        auto &joint = joints.add();
        joint.jointArrayIndex = joints.size() - 1;
        
        // set gcHandle info
        joint.gcHandleIndex = item.gcHandleArrayIndex;
        
        crawlManagedEntryAddress(item.target, nullptr, *__memoryReader, joint, false, 0);
    }
    __sampler.end();
}

void MemorySnapshotCrawler::crawlStatic()
{
    __sampler.begin("crawlStatic");
    auto &typeDescriptions = *snapshot.typeDescriptions;
    for (auto i = 0; i < typeDescriptions.size; i++)
    {
        auto &type = typeDescriptions[i];
        if (type.staticFieldBytes == nullptr || type.staticFieldBytes->size == 0){continue;}
        for (auto n = 0; n < type.fields->size; n++)
        {
            auto &field = type.fields->items[n];
            if (!field.isStatic){continue;}
            
            __staticMemoryReader->load(*type.staticFieldBytes);
            
            HeapMemoryReader *reader;
            address_t fieldAddress = 0;
            auto *fieldType = &snapshot.typeDescriptions->items[field.typeIndex];
            if (fieldType->isValueType)
            {
                fieldAddress = field.offset - __vm->objectHeaderSize;
                reader = __staticMemoryReader;
            }
            else
            {
                fieldAddress = __staticMemoryReader->readPointer(field.offset);
                reader = __memoryReader;
            }
            
            auto &joint = joints.add();
            joint.jointArrayIndex = joints.size() - 1;
            
            // set static field info
            joint.hookTypeIndex = type.typeIndex;
            joint.fieldSlotIndex = field.fieldSlotIndex;
            joint.fieldTypeIndex = field.typeIndex;
            joint.isStatic = true;
            
            crawlManagedEntryAddress(fieldAddress, fieldType, *reader, joint, false, 0);
        }
    }
    __sampler.end();
}

MemorySnapshotCrawler::~MemorySnapshotCrawler()
{
    delete __mirror;
    delete __memoryReader;
    delete __staticMemoryReader;
}
