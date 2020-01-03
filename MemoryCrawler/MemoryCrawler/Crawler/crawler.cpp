//
//  crawler.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/5.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "crawler.h"

std::string comma(uint64_t v, uint32_t width = 0);

MemorySnapshotCrawler::MemorySnapshotCrawler()
{
    
}

MemorySnapshotCrawler &MemorySnapshotCrawler::crawl()
{
    __sampler.begin("MemorySnapshotCrawler");
    prepare();
    crawlGCHandles();
    crawlStatic();
    crawlLinks();
    summarize();
    __sampler.end();
#if PERF_DEBUG
    __sampler.summarize();
#endif
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
    __sampler.begin("SummarizeManagedObjects");
    auto &nativeObjects = *snapshot->nativeObjects;
    auto &typeDescriptions = *snapshot->typeDescriptions;
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
    __sampler.begin("Prepare");
    __sampler.begin("InitManagedTypes");
    Array<TypeDescription> &typeDescriptions = *snapshot->typeDescriptions;
    for (auto i = 0; i < typeDescriptions.size; i++)
    {
        TypeDescription &type = typeDescriptions[i];
        type.isUnityEngineObjectType = deriveFromMType(type, snapshot->managedTypeIndex.unityengine_Object);
    }
    __sampler.end();
    __sampler.begin("InitNativeConnections");
    
    auto offset = snapshot->gcHandles->size;
    auto &nativeConnections = *snapshot->connections;
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
            auto &type = crawler.snapshot->typeDescriptions->items[mo.typeIndex];
            container.insert(pair<address_t, int64_t>(mo.address, type.typeInfoAddress));
        }
    }
    
    for (auto i = 0; i < managedObjects.size(); i++)
    {
        auto &mo = managedObjects[i];
        if (!mo.isValueType)
        {
            auto &type = snapshot->typeDescriptions->items[mo.typeIndex];
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
    for (auto i = 0; i < crawler.snapshot->nativeObjects->size; i++)
    {
        auto &no = crawler.snapshot->nativeObjects->items[i];
        container.insert(pair<address_t, int32_t>(no.nativeObjectAddress, no.nativeTypeArrayIndex));
    }
    
    for (auto i = 0; i < snapshot->nativeObjects->size; i++)
    {
        auto &no = snapshot->nativeObjects->items[i];
        auto iter = container.find(no.nativeObjectAddress);
        
        if (iter == container.end())
        {
            no.state = MS_allocated;
        }
        else
        {
            auto &typeA = crawler.snapshot->nativeTypes->items[iter->second];
            auto &typeB = snapshot->nativeTypes->items[no.nativeTypeArrayIndex];
            no.state = typeA.name == typeB.name ? MS_persistent : MS_allocated;
        }
    }
    
    // Compare Memory
    __concations.clear();
    auto &sections = *snapshot->sortedHeapSections;
    auto &referSections = *crawler.snapshot->sortedHeapSections;
    auto position = 0;
    for (auto i = 0; i < sections.size(); i++)
    {
        auto s = sections[i];
        MemoryConcation concat(s->startAddress, s->size, i, CT_IDENTICAL);
        while(position < referSections.size())
        {
            auto rs = referSections[position];
            if (rs->startAddress < s->startAddress)
            {
                ++position;
                __concations.emplace_back(MemoryConcation(rs->startAddress, rs->size, position, CT_DEALLOC));
            }
            else if (rs->startAddress >= s->startAddress && rs->startAddress + rs->size <= s->startAddress + s->size)
            {
                concat.fragments.emplace_back(MemoryFragment(rs->startAddress, rs->size, position));
                ++position;
            } else {break;}
        }
        switch (concat.fragments.size())
        {
            case 0:
            {
                concat.type = CT_ALLOC;
            } break;
            
            case 1:
            {
                auto &frag = concat.fragments[0];
                concat.type = (frag.address == concat.address && frag.size == concat.size) ? CT_IDENTICAL : CT_CONCAT;
            } break;
            
            default:
            {
                concat.type = CT_CONCAT;
            } break;
        }
        __concations.emplace_back(concat);
    }
}

void MemorySnapshotCrawler::dumpAllClasses()
{
    auto &typeDescriptions = snapshot->typeDescriptions->items;
    for (auto i = 0; i < snapshot->typeDescriptions->size; i++)
    {
        auto &type = typeDescriptions[i];
        printf("\e[36m%d \e[32m%s \e[37m%s\n", type.typeIndex, type.name.c_str(), type.assembly.c_str());
    }
}

bool MemorySnapshotCrawler::search(std::string &keyword, std::string &content, bool reverseSearching)
{
    if (content.size() < keyword.size()) {return false;}
    if (reverseSearching)
    {
        auto k = keyword.rbegin();
        auto c = content.rbegin();
        while (k != keyword.rend())
        {
            if (*k != *c) { return false; }
            ++k;
            ++c;
        }
    }
    else
    {
        auto k = keyword.begin();
        auto c = content.begin();
        while (k != keyword.end())
        {
            if (*k != *c) { return false; }
            ++k;
            ++c;
        }
    }
    return true;
}

void MemorySnapshotCrawler::findNObject(string name, bool reverseMatching)
{
    auto &nativeObjects = snapshot->nativeObjects->items;
    for (auto i = 0; i < snapshot->nativeObjects->size; i++)
    {
        auto &no = nativeObjects[i];
        auto &nt = snapshot->nativeTypes->items[no.nativeTypeArrayIndex];
        if (search(name, no.name, reverseMatching))
        {
            printf("\e[36m0x%llx \e[32m'%s' \e[33m%s\n", no.nativeObjectAddress, no.name.c_str(), nt.name.c_str());
        }
    }
}

void MemorySnapshotCrawler::findClass(string name, bool reverseMatching)
{
    auto &typeDescriptions = snapshot->typeDescriptions->items;
    for (auto i = 0; i < snapshot->typeDescriptions->size; i++)
    {
        auto &type = typeDescriptions[i];
        if (search(name, type.name, reverseMatching))
        {
            printf("\e[36m%d \e[32m%s \e[37m%s\n", type.typeIndex, type.name.c_str(), type.assembly.c_str());
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
                        auto &type = snapshot->typeDescriptions->items[typeIndex];
                        switch (itemIndex)
                        {
                            case -1:
                            {
                                printf("│ [%s] memory=%d type_index=%d\n", type.name.c_str(), (int32_t)size, type.typeIndex);
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
                                printf("│ 0x%08llx %6d %s\n", mo.address, mo.size, type.name.c_str());
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
    for (auto i = 0; i < snapshot->nativeObjects->size; i++)
    {
        auto &no = snapshot->nativeObjects->items[i];
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
                        auto &type = snapshot->nativeTypes->items[typeIndex];
                        switch (itemIndex)
                        {
                            case -1:
                            {
                                printf("│ [%s] memory=%d type_index=%d\n", type.name.c_str(), (int32_t)size, type.typeIndex);
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
                                auto &no = snapshot->nativeObjects->items[itemIndex];
                                printf(format, no.nativeObjectAddress, no.size, no.name.c_str());
                                break;
                            }
                        }
                    }, depth);
    printf("\e[37m│ [SUMMARY] count=%d memory=%d\n", count, total);
    printf("└%s\n", sep);
}

void MemorySnapshotCrawler::trackMTypeObjects(MemoryState state, int32_t typeIndex, int32_t rank, int32_t depth)
{
    auto maxValue = 0;
    TrackStatistics objects;
    for (auto i = 0; i < managedObjects.size(); i++)
    {
        auto &mo = managedObjects[i];
        if (mo.typeIndex == typeIndex && (state == MS_none || state == mo.state))
        {
            objects.collect(i, mo.typeIndex, mo.size);
            if (mo.size > maxValue) { maxValue = mo.size; }
        }
    }
    
    objects.summarize(false);
    
    auto &type = snapshot->typeDescriptions->items[typeIndex];
    if (type.typeIndex != typeIndex) {return;}
    printf("\e[32m%s \e[37m%s \e[36m*%d\n", type.name.c_str(), type.assembly.c_str(), type.typeIndex);
    
    vector<int32_t> indice;
    
    int32_t total = 0;
    int32_t count = 0;
    objects.foreach([&](int32_t itemIndex, int32_t typeIndex, int32_t size, uint64_t detail)
                    {
                        if (itemIndex >= 0)
                        {
                            count++;
                            total += size;
                            indice.push_back(itemIndex);
                        }
                    }, 0);
    
    
    auto digitCount = indice.size() == 0 ? 2 : (int32_t)ceil(log10(maxValue));
    
    auto listCount = 0;
    for (auto i = indice.begin(); i != indice.end(); i++)
    {
        if (rank > 0 && listCount++ >= rank) {break;}
        
        auto &mo = managedObjects[*i];
        auto relation = getMRefNode(&mo, depth);
        printf("\e[36m0x%08llx %s ⤽[", mo.address, comma(mo.size, digitCount).c_str());
        if (relation != nullptr)
        {
            ManagedObject *node = nullptr;
            switch (relation->fromKind)
            {
                case CK_link:
                {
                    auto appending = snapshot->nativeAppendingCollection.appendings[relation->from];
                    auto index = findMObjectAtAddress(appending.link.managedAddress);
                    if (index >= 0)
                    {
                        node = &managedObjects[index];
                        printf("<LINK>::");
                    }
                } break;
                    
                case CK_gcHandle:
                {
                    auto address = snapshot->gcHandles->items[relation->from].target;
                    auto index = findMObjectAtAddress(address);
                    if (index >= 0)
                    {
                        node = &managedObjects[index];
                        printf("<GCHandle>::");
                    }
                } break;
                    
                case CK_static:
                {
                    auto &ej = joints[relation->jointArrayIndex];
                    auto &type = snapshot->typeDescriptions->items[ej.hookTypeIndex];
                    auto &field = type.fields->items[ej.fieldSlotIndex];
                    printf("<Static>::%s::%s", type.name.c_str(), field.name.c_str());
                } break;
                
                case CK_managed:
                {
                    if (relation->from != mo.managedObjectIndex) {node = &managedObjects[relation->from];}
                } break;
                    
                default: break;
            }
            
            if (node != nullptr)
            {
                auto &tagType = snapshot->typeDescriptions->items[node->typeIndex];
                printf("0x%08llx type='%s'%d", node->address, tagType.name.c_str(), tagType.typeIndex);
            }
            else if (relation->fromKind != CK_static)
            {
                printf("NULL");
            }
        }
        else
        {
            printf("NULL");
        }
        printf("]");
        if (type.isUnityEngineObjectType)
        {
            auto nAddress = findNObjectOfMObject(mo.address);
            if (nAddress != 0)
            {
                auto index = __nativeObjectAddressMap.at(nAddress);
                assert(index >= 0);
                
                auto &no = snapshot->nativeObjects->items[index];
                auto &nt = snapshot->nativeTypes->items[no.nativeTypeArrayIndex];
                printf(" \e[32m*{0x%llx %s \e[33m'%s'\e[32m %s}", no.nativeObjectAddress, nt.name.c_str(), no.name.c_str(), comma(no.size).c_str());
            }
        }
        printf("\n");
    }
    
    printf("\e[37m[SUMMARY] total_count=%d memory=%d\n", count, total);
}

void MemorySnapshotCrawler::trackNTypeObjects(MemoryState state, int32_t typeIndex, int32_t rank)
{
    auto maxValue = 0;
    TrackStatistics objects;
    for (auto i = 0; i < snapshot->nativeObjects->size; i++)
    {
        auto &no = snapshot->nativeObjects->items[i];
        if (no.nativeTypeArrayIndex == typeIndex && (state == MS_none || state == no.state))
        {
            objects.collect(i, no.nativeTypeArrayIndex, no.size);
            if (no.size > maxValue) { maxValue = no.size; }
        }
    }
    
    objects.summarize(false);
    
    vector<int32_t> indice;
    
    int32_t total = 0;
    int32_t count = 0;
    objects.foreach([&](int32_t itemIndex, int32_t typeIndex, int32_t size, uint64_t detail)
                    {
                        if (itemIndex >= 0)
                        {
                            count++;
                            total += size;
                            indice.push_back(itemIndex);
                        }
                    }, 0);
    auto digitCount = indice.size() == 0 ? 2 : (int32_t)ceil(log10(maxValue));
    auto &collection = snapshot->nativeAppendingCollection;
    auto listCount = 0;
    for (auto i = indice.begin(); i != indice.end(); i++)
    {
        if (rank > 0 && listCount++ >= rank) {break;}
        
        auto &no = snapshot->nativeObjects->items[*i];
        auto &type = snapshot->nativeTypes->items[no.nativeTypeArrayIndex];
        printf("\e[36m0x%08llx %s %s \e[33m'%s'\e[36m", no.nativeObjectAddress, comma(no.size, digitCount).c_str(), type.name.c_str(), no.name.c_str());
        if (collection.appendings.size() > 0)
        {
            auto &appending = collection.appendings[no.nativeObjectArrayIndex];
            if (typeIndex == snapshot->nativeTypeIndex.Sprite)
            {
                auto &sprite = collection.sprites[appending.sprite];
                printf(" rect={x=%.0f, y=%.0f, width=%.0f, height=%.0f} pivot={x=%.2f, y=%.2f}",
                       sprite.x, sprite.y, sprite.width, sprite.height, sprite.pivot.x, sprite.pivot.y);
                if (sprite.textureNativeArrayIndex >= 0)
                {
                    auto &texture = snapshot->nativeObjects->items[sprite.textureNativeArrayIndex];
                    printf(" Texture2D[0x%llx \e[33m'%s'\e[36m]", texture.nativeObjectAddress, texture.name.c_str());
                }
            }
            else if (typeIndex == snapshot->nativeTypeIndex.Texture2D)
            {
                auto &tex = collection.textures[appending.texture];
                printf(" pot=%s format=%d %dx%d", tex.pot? "true" : "false", tex.format, tex.width, tex.height);
            }
        }
        auto mAddress = findMObjectOfNObject(no.nativeObjectAddress);
        if (mAddress != 0)
        {
            auto mindex = findMObjectAtAddress(mAddress);
            auto &mo = managedObjects[mindex];
            auto &mt = snapshot->typeDescriptions->items[mo.typeIndex];
            printf(" \e[32m*{0x%llx %s}", mo.address, mt.name.c_str());
        }
        printf("\n");
    }
    printf("\e[37m[SUMMARY] total_count=%d memory=%d\n", count, total);
}

void MemorySnapshotCrawler::topMObjects(int32_t rank, address_t address, bool keepAddressOrder)
{
    MemorySection *section = nullptr;
    auto &heapSections = *snapshot->sortedHeapSections;
    for (auto iter = heapSections.begin(); iter != heapSections.end(); iter++)
    {
        auto &s = **iter;
        if (s.startAddress == address)
        {
            section = &s;
            break;
        }
    }
    
    std::vector<ManagedObject *> objects;
    if (section == nullptr)
    {
        for (auto i = 0; i < managedObjects.size(); i++)
        {
            auto &mo = managedObjects[i];
            if (mo.size <= 0 || mo.address <= 0xFFFF || mo.isValueType) {continue;}
            objects.push_back(&mo);
        }
    }
    else
    {
        for (auto i = 0; i < managedObjects.size(); i++)
        {
            auto &mo = managedObjects[i];
            if (mo.size <= 0 || mo.address <= 0xFFFF || mo.isValueType) {continue;}
            if (mo.address < section->startAddress || mo.address >= section->startAddress + section->size) {continue;}
            objects.push_back(&mo);
        }
    }
    
    std::sort(objects.begin(), objects.end(), [](ManagedObject *a, ManagedObject *b)
    {
        if (a->size != b->size)
        {
            return a->size > b->size;
        }
        return a->address < b->address;
    });
    
    if (rank > objects.size() || rank <= 0) { rank = (int32_t)objects.size(); }
    
    if (keepAddressOrder)
    {
        std::sort(objects.begin(), objects.begin() + rank, [](ManagedObject *a, ManagedObject *b)
        {
            return a->address < b->address;
        });
    }
    
    address_t prevAddress = 0;
    for (auto i = 0; i < rank; i++)
    {
        auto mo = objects[i];
        if (i >= objects.size()) {break;}
        
        auto &type = snapshot->typeDescriptions->items[mo->typeIndex];
        printf("\e[36m0x%llx \e[32m%s \e[36m%s", mo->address, type.name.c_str(), comma(mo->size).c_str());
        if (keepAddressOrder && prevAddress != 0) { printf(" +%lld", mo->address - prevAddress); }
        printf("\n");
        prevAddress = mo->address + mo->size;
    }
}

void MemorySnapshotCrawler::topNObjects(int32_t rank)
{
    std::vector<PackedNativeUnityEngineObject *> objects;
    for (auto i = 0; i < snapshot->nativeObjects->size; i++)
    {
        auto &no = snapshot->nativeObjects->items[i];
        objects.push_back(&no);
    }
    
    std::sort(objects.begin(), objects.end(), [](PackedNativeUnityEngineObject *a, PackedNativeUnityEngineObject *b)
              {
                  if (a->size != b->size)
                  {
                      return a->size > b->size;
                  }
                  return a->nativeObjectAddress < b->nativeObjectAddress;
              });
    for (auto i = 0; i < rank; i++)
    {
        auto no = objects[i];
        if (i >= objects.size()) {break;}
        
        auto &type = snapshot->nativeTypes->items[no->nativeTypeArrayIndex];
        printf("\e[36m0x%llx \e[32m%s \e[33m'%s' \e[36m%s\n", no->nativeObjectAddress, type.name.c_str(), no->name.c_str(), comma(no->size).c_str());
    }
}

void MemorySnapshotCrawler::barMMemory(MemoryState state, int32_t rank)
{
    int64_t totalMemory = 0;
    vector<int32_t> indice;
    map<int32_t, int64_t> typeMemory;
    map<int32_t, int64_t> typeCount;
    
    for (auto i = 0; i < managedObjects.size(); i++)
    {
        auto &mo = managedObjects[i];
        if (state == MS_none || mo.state == state)
        {
            assert(mo.typeIndex >= 0);
            auto &type = snapshot->typeDescriptions->items[mo.typeIndex];
            if (type.isValueType) {continue;}
            totalMemory += mo.size;
            auto typeIndex = mo.typeIndex;
            auto iter = typeMemory.find(typeIndex);
            if (iter == typeMemory.end())
            {
                iter = typeMemory.insert(std::make_pair(typeIndex, 0)).first;
                indice.push_back(typeIndex);
                
                typeCount.insert(std::make_pair(typeIndex, 0));
            }
            
            iter->second += mo.size;
            typeCount.at(typeIndex)++;
        }
    }
    
    printf("### total=%s\n", comma(totalMemory).c_str());
    
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
        percent = 100 * (typeMemory.at(index) / static_cast<double>(totalMemory));
        accumulation += percent;
        stats.push_back(std::make_tuple(index, percent, accumulation));
    }
    
    char progress[300+1];
    char fence[] = "█";
    auto &managedTypes = *snapshot->typeDescriptions;
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
        printf("%s %s %s #%lld *%d\n", progress, type.name.c_str(), comma(typeMemory.at(typeIndex)).c_str(), typeCount.at(typeIndex), typeIndex);
    }
}

void MemorySnapshotCrawler::barNMemory(MemoryState state, int32_t rank)
{
    double totalMemory = 0;
    vector<int32_t> indice;
    map<int32_t, int32_t> typeMemory;
    map<int32_t, int32_t> typeCount;
    
    auto &nativeObjects = *snapshot->nativeObjects;
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
    
    printf("### total=%s\n", comma(totalMemory).c_str());
    
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
    auto &nativeTypes = *snapshot->nativeTypes;
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
        printf("%s %s %s #%d *%d\n", progress, type.name.c_str(), comma(typeMemory.at(typeIndex)).c_str(), typeCount.at(typeIndex), typeIndex);
    }
}

void MemorySnapshotCrawler::statFragments()
{
    auto maxsize = 0;
    for (auto i = __concations.begin(); i != __concations.end(); i++)
    {
        if (maxsize < i->size) { maxsize = i->size; }
    }
    
    auto memwidth = ceil(log10(fmax(10, maxsize)));
    auto fragAddition = 0, allocAddition = 0, deallocations = 0;
    for (auto i = __concations.begin(); i != __concations.end(); i++)
    {
        auto &concat = *i;
        printf("[%03d] 0x%llx %s ", concat.index, concat.address, comma(concat.size, memwidth).c_str());
        switch (concat.type)
        {
            case CT_IDENTICAL:
                printf("IDENTICAL\n");
                break;
                
            case CT_CONCAT:
            {
                auto maxsize = 0;
                auto fragCount = 0;
                for (auto f = concat.fragments.begin(); f != concat.fragments.end(); f++)
                {
                    if (f->size > maxsize) { maxsize = f->size; }
                    fragCount += f->size;
                }
                fragAddition += concat.size - fragCount;
                printf("CONCAT=%lu +%s=%dK\n", concat.fragments.size(), comma(concat.size - fragCount).c_str(), (concat.size - fragCount)/1024);
                
                auto width = ceil(log10(fmax(10, maxsize)));
                for (auto f = concat.fragments.begin(); f != concat.fragments.end(); f++)
                {
                    auto &fragment = *f;
                    printf("    - [%03d] 0x%llx %s\n", fragment.index, fragment.address, comma(fragment.size, width).c_str());
                }
            } break;
                
            case CT_ALLOC:
            {
                printf("ALLOC\n");
                allocAddition += concat.size;
            } break;
                
            case CT_DEALLOC:
            {
                deallocations += concat.size;
            } break;
        }
    }
    
    printf("[SUMMARY] fragments+%s=%dK alloc+%s=%dK dealloc+%s=%dK\n", comma(fragAddition).c_str(), fragAddition/1024, comma(allocAddition).c_str(), allocAddition/1024, comma(deallocations).c_str(), deallocations/1024);
}

void MemorySnapshotCrawler::drawUsedHeapGraph(const char *filename, bool sort)
{
    const double rowWidth = 1920, rowHeight = 100, gap = 5;
    
    int64_t length = 0;
    auto &heapSections = *snapshot->sortedHeapSections;
    for (auto iter = heapSections.begin(); iter!=heapSections.end(); iter++)
    {
        auto &section = **iter;
        if (section.size > length) { length = section.size; }
    }
    
    std::map<address_t, ManagedObject *> map;
    for (auto i = 0; i < managedObjects.size(); i++)
    {
        auto &mo = managedObjects[i];
        if (mo.size <= 0 || mo.address <= 0xFFFF || mo.isValueType) {continue;}
        map.insert(std::make_pair(mo.address, &mo));
    }

    auto section = heapSections.begin();
    std::map<int32_t, std::vector<Rectangle>> stacks;
    std::vector<Rectangle> *children = &stacks.insert(std::make_pair((**section).heapArrayIndex, std::vector<Rectangle>())).first->second;
    
    Rectangle back;
    double cursorY = 0;
    for (auto iter = map.begin(); iter != map.end(); iter++)
    {
        auto &mo = iter->second;
        
        auto s = *section;
        while (mo->address >= s->startAddress + s->size)
        {
            section++;
            assert(section != heapSections.end());
            children = &stacks.insert(std::make_pair((**section).heapArrayIndex, std::vector<Rectangle>())).first->second;
            cursorY += rowHeight + gap;
            s = *section;
        }
        
        assert(mo->address >= s->startAddress);
        
        Rectangle rect(rowWidth * (mo->address - s->startAddress) / length, cursorY, rowWidth * mo->size / length, rowHeight);
        assert(rect.x < s->size * rowWidth / length);
        
        if (back.width != 0 && back ^ rect)
        {
            back + rect;
            memcpy(&children->back(), &back, sizeof(Rectangle));
        }
        else
        {
            memcpy(&back, &rect, sizeof(Rectangle));
            children->push_back(back);
        }
    }
    
    const double canvasWidth = rowWidth, canvasHeight = cursorY + rowHeight;
    
    char str[512];
    auto ptr = str;
    
    mkdir("__graph", 0777);
    sprintf(ptr, "__graph/%s_draw+%lldK.svg", filename, length >> 10);
    
    FileStream fs;
    fs.open(ptr, std::fstream::out | std::fstream::trunc | std::fstream::binary);
    fs.write("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\"");
    sprintf(ptr, " width=\"%.0f\" height=\"%.0f\">\n", canvasWidth, canvasHeight);
    fs.write((const char *)ptr);
    
    using Iterator = decltype(stacks.begin());
    std::vector<Iterator> indices;
    for (auto iter = stacks.begin(); iter != stacks.end(); iter++) { indices.push_back(iter); }
    if(sort)
    {
        std::sort(indices.begin(), indices.end(), [&](Iterator a, Iterator b)
        {
            return heapSections[a->first]->size > heapSections[b->first]->size;
        });
    }
    
    cursorY = 0;
    for (auto i = 0; i < indices.size(); i++)
    {
        Iterator iter = indices[i];
        auto &section = *heapSections[iter->first];
        sprintf(ptr, "<rect x=\"%.3f\" y=\"%.3f\" width=\"%.3f\" height=\"%.3f\" stroke=\"none\" fill=\"lightgray\"/>\n", 0.0, cursorY, rowWidth, rowHeight);
        fs.write((const char *)ptr);
        sprintf(ptr, "<rect x=\"%.3f\" y=\"%.3f\" width=\"%.3f\" height=\"%.3f\" stroke=\"none\" fill=\"red\"/>\n", 0.0, cursorY, rowWidth * section.size / length, rowHeight);
        fs.write((const char *)ptr);

        sprintf(ptr, "<text x=\"%.3f\" y=\"%.3f\" fill=\"darkred\" font-family=\"Courier\" font-size=\"20\" opacity=\"0.2\">0x%llx</text>\n", 0.0, cursorY + 20, section.startAddress);
        fs.write((const char *)ptr);
        
        auto scale = 0.75;
        auto &blocks = iter->second;
        for (auto c = blocks.begin(); c != blocks.end(); c++)
        {
            assert(c->x < section.size * rowWidth / length);
            sprintf(ptr, "<rect x=\"%.3f\" y=\"%.3f\" width=\"%.3f\" height=\"%.3f\" stroke=\"none\" fill=\"gold\"/>\n", c->x, cursorY + c->height * (1 - scale), c->width, c->height * scale);
            fs.write((const char *)ptr);
        }
        
        cursorY += rowHeight + gap;
    }
    
    fs.write("</svg>");
    fs.close();
}

void MemorySnapshotCrawler::drawHeapGraph(const char *filename, bool comparisonEnabled)
{
    const double canvasWidth = 1920, canvasHeight = 1080;
    const double gap = 5;
    
    std::vector<MemoryFragment> fragments;
    auto &heapSections = *snapshot->sortedHeapSections;
    for (auto iter = heapSections.begin(); iter!=heapSections.end(); iter++)
    {
        auto &section = **iter;
        fragments.push_back(MemoryFragment(section.startAddress, section.size, (int32_t)fragments.size()));
    }
    
    std::vector<MemoryFragment> __fragments;
    if (comparisonEnabled)
    {
        for (auto iter = __concations.begin(); iter != __concations.end(); iter++)
        {
            if (iter->type == CT_DEALLOC)
            {
                __fragments.emplace_back(*iter);
            }
            else
            {
                for (auto f = iter->fragments.begin(); f != iter->fragments.end(); f++)
                {
                    __fragments.emplace_back(*f);
                }
            }
        }
    }
    
    std::vector<std::vector<MemoryFragment> *> sources{ &fragments };
    
    auto front = &fragments.front();
    auto back = &fragments.back();
    if (comparisonEnabled)
    {
        if (__fragments.front().address < front->address)
        {
            front = &__fragments.front();
        }
        
        if (__fragments.back().address > back->address + back->size)
        {
            back = &__fragments.back();
        }
        
        sources.push_back(&__fragments);
    }
    
    const int64_t length = 1 << 27; // 128MB
    const int64_t offset = front->address;
    const int64_t magnitude = (back->address + back->size) - offset;
    const int64_t rowCount = magnitude / length + (magnitude % length > 0 ? 1 : 0);
    const double rowHeight = (canvasHeight - (rowCount - 1) * gap) / rowCount;
    
    std::vector<std::vector<Rectangle>> layers(sources.size());
    for (auto n = 0; n < sources.size(); n++)
    {
        auto &blocks = layers[n];
        auto provider = sources[n];
        
        int64_t position = 0;
        double cursorX = 0, cursorY = 0;
        for (auto i = 0; i < provider->size(); i++)
        {
            auto &section = (*provider)[i];
            auto s = section.address - offset - position;
            while (s >= length)
            {
                position += length;
                s = section.address - offset - position;
                cursorY += gap + rowHeight;
                cursorX = 0;
            }
            
            blocks.push_back(Rectangle((double)s * canvasWidth / length, cursorY,
                                        fmin(section.size, length - s) * canvasWidth / length, rowHeight));
            
            int64_t r = (int64_t)(s + section.size) - length;
            while (r >= 0)
            {
                position += length;
                cursorY += gap + rowHeight;
                cursorX = 0;
                if (r > 0)
                {
                    blocks.push_back(Rectangle(0, cursorY,
                                               fmin(r, length) * canvasWidth/length, rowHeight));
                }
                r -= length;
            }
        }
    }
    
    double cursorY = 0;
    std::vector<Rectangle> regions;
    for (auto i = 0; i < rowCount; i++)
    {
        regions.push_back(Rectangle(0, cursorY, canvasWidth, rowHeight));
        cursorY += gap + rowHeight;
    }
    
    char str[512];
    auto ptr = str;
    
    mkdir("__graph", 0777);
    sprintf(ptr, "__graph/%s.svg", filename);
    
    FileStream fs;
    fs.open(ptr, std::fstream::out | std::fstream::trunc | std::fstream::binary);
    fs.write("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\"");
    sprintf(ptr, " width=\"%.0f\" height=\"%.0f\">\n", canvasWidth, canvasHeight);
    fs.write((const char *)ptr);
    auto index = 0;
    for (auto iter = regions.begin(); iter != regions.end(); iter++)
    {
        sprintf(ptr, "<rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" stroke=\"none\" fill=\"lightgray\"/>\n", iter->x, iter->y, iter->width, iter->height);
        fs.write((const char *)ptr);
        
        auto address = offset + index * length;
        sprintf(ptr, "<text x=\"%.2f\" y=\"%.2f\" fill=\"gray\" font-family=\"Courier\" font-size=\"20\" opacity=\"0.2\">0x%llx</text>\n", iter->x, iter->y + 20, address);
        fs.write((const char *)ptr);
        ++index;
    }
    
    std::vector<const char *> colors {"red", "gold"};
    std::vector<float> scales {1.0, 0.5};
    
    for (auto i = 0; i < layers.size(); i++)
    {
        auto &blocks = layers[i];
        auto color = colors[i];
        auto scale = scales[i];
        for (auto iter = blocks.begin(); iter != blocks.end(); iter++)
        {
            sprintf(ptr, "<rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" stroke=\"none\" fill=\"%s\"/>\n", iter->x, iter->y + iter->height * (1 - scale), iter->width, iter->height * scale, color);
            fs.write((const char *)ptr);
        }
    }
    
    fs.write("</svg>");
    fs.close();
}

void MemorySnapshotCrawler::inspectHeap(const char *filename)
{
    auto &heapSections = *snapshot->sortedHeapSections;
    
    auto const COL_COUNT = 3;
    auto const ROW_COUNT = (int32_t)ceil((double)heapSections.size() / (double)COL_COUNT);
    
    auto maxShift = 0, maxSize = 0;
    for (auto i = 0; i < heapSections.size(); i++)
    {
        auto &item = *heapSections[i];
        auto shift = 0;
        if (i != 0)
        {
            auto &base = *heapSections[i-1];
            shift = (int32_t)(item.startAddress - (base.startAddress + base.size));
        }
        
        if (maxShift < shift) { maxShift = shift; }
        if (maxSize < item.size) { maxSize = item.size; }
    }
    
    auto sizew = (int32_t)ceil(log10(fmax(10, maxSize / 1024)));
    auto shiftw = (int32_t)ceil(log10(fmax(10, maxShift)));
    
    auto len = 0;
    auto indexw = (int32_t)ceil(log10(fmax(10, heapSections.size())));
    char indexf[indexw+4];
    char indexb[indexw+1];
    len = sprintf(indexf, "%%%dd", indexw);
    memset(indexf+len, 0, 1);
    
    for (auto r = 0; r < ROW_COUNT; r++)
    {
        for (auto c = 0; c < COL_COUNT; c++)
        {
            auto index = r + c * ROW_COUNT;
            if (index >= heapSections.size()) {break;}
            auto &item = *heapSections[index];
            int64_t shift = 0;
            if (index > 0)
            {
                auto &base = *heapSections[index-1];
                shift = (int64_t)(item.startAddress - (base.startAddress + base.size));
            }
            sprintf(indexb, indexf, index);
            printf(" %s 0x%llx %s %sK |", indexb, item.startAddress, comma(shift, shiftw).c_str(), comma(item.size/1024, sizew).c_str());
        }
        printf("\n");
    }
    
    if (filename != nullptr && strlen(filename) > 0)
    {
        char basepath[256];
        auto ptr = basepath;
        ptr += sprintf(ptr, "%s", "__heap");
        ptr += sprintf(ptr, "+%s", filename);
        mkdir(basepath, 0766);
        
        std::fstream fs;
        char filepath[sizeof(basepath) + 32];
        for (auto i = 0; i < heapSections.size(); i++)
        {
            auto &section = *heapSections[i];
            sprintf(filepath, "%s/%llx_%d.mem", basepath, section.startAddress, section.bytes->size);
            
            fs.open(filepath, std::fstream::out | std::fstream::trunc);
            fs.write((char *)section.bytes->items, section.bytes->size);
            fs.flush();
            fs.close();
            printf("%4d %s\n", i, filepath);
        }
    }
}

void MemorySnapshotCrawler::statHeap(int32_t rank)
{
    using std::get;
    
    auto totalMemory = 0;
    map<int32_t, int32_t> sizeMemory;
    map<int32_t, int32_t> sizeCount;
    
    auto maxsize = 0;
    vector<int32_t> indice;
    auto &sortedHeapSections = *snapshot->sortedHeapSections;
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
        if (heap.size > maxsize) { maxsize = heap.size; }
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
    
    char percentage[600+1];
    char fence[] = "█";
    printf("### blocks=%d memory=%s\n", (int32_t)sortedHeapSections.size(), comma(totalMemory).c_str());
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

void MemorySnapshotCrawler::dumpSubclassesOf(int32_t typeIndex)
{
    auto &base = snapshot->typeDescriptions->items[typeIndex];
    if (base.typeIndex != typeIndex) {return;}
    printf("# %s *%d\n", base.name.c_str(), base.typeIndex);
    auto &typeDescriptions = snapshot->typeDescriptions->items;
    for (auto i = 0; i < snapshot->typeDescriptions->size; i++)
    {
        auto &type = typeDescriptions[i];
        if (subclassOf(type, typeIndex))
        {
            printf("\e[32m%s \e[33m%s \e[37m*%d\n", type.name.c_str(), type.assembly.c_str(), type.typeIndex);
        }
    }
}

void MemorySnapshotCrawler::statSubclasses()
{
    vector<int32_t> indice;
    map<int32_t, int32_t> stats;
    auto &typeDescriptions = snapshot->typeDescriptions->items;
    for (auto i = 0; i < snapshot->typeDescriptions->size; i++)
    {
        auto type = &typeDescriptions[i];
        while (type->baseOrElementTypeIndex >= 0)
        {
            auto match = stats.find(type->baseOrElementTypeIndex);
            if (match == stats.end())
            {
                stats.insert(pair<int32_t, int32_t>(type->baseOrElementTypeIndex, 1));
                indice.push_back(type->baseOrElementTypeIndex);
            }
            else
            {
                match->second++;
            }
            
            type = &snapshot->typeDescriptions->items[type->baseOrElementTypeIndex];
        }
    }
    
    std::sort(indice.begin(), indice.end(), [&](int32_t a, int32_t b)
              {
                  auto na = stats[a];
                  auto nb = stats[b];
                  if (na != nb) {return na > nb;}
                  return a < b;
              });
    
    for (auto i = indice.begin(); i != indice.end(); ++i)
    {
        auto &type = snapshot->typeDescriptions->items[*i];
        printf("\e[36m#%d \e[32m%s \e[33m%s \e[37m*%d\n", stats[*i], type.name.c_str(), type.assembly.c_str(), type.typeIndex);
    }
}

bool MemorySnapshotCrawler::subclassOf(TypeDescription &type, int32_t baseTypeIndex)
{
    if (baseTypeIndex < 0) {return false;}
    
    auto iter = &type;
    while (iter->baseOrElementTypeIndex >= 0 && !iter->isArray)
    {
        if (iter->baseOrElementTypeIndex == baseTypeIndex) {return true;}
        iter = &snapshot->typeDescriptions->items[iter->baseOrElementTypeIndex];
    }
    
    return false;
}

bool MemorySnapshotCrawler::deriveFromMType(TypeDescription &type, int32_t baseTypeIndex)
{
    if (type.typeIndex == baseTypeIndex) { return true; }
    if (type.typeIndex < 0 || baseTypeIndex < 0) { return false; }
    
    TypeDescription *iter = &type;
    while (iter->baseOrElementTypeIndex != -1 && !iter->isArray)
    {
        iter = &snapshot->typeDescriptions->items[iter->baseOrElementTypeIndex];
        if (iter->typeIndex == baseTypeIndex) { return true; }
    }
    
    return false;
}

bool MemorySnapshotCrawler::deriveFromNType(PackedNativeType &type, int32_t baseTypeIndex)
{
    if (type.typeIndex == baseTypeIndex) { return true; }
    if (type.typeIndex < 0 || baseTypeIndex < 0) { return false; }
    
    PackedNativeType *iter = &type;
    while (iter->nativeBaseTypeArrayIndex != -1)
    {
        iter = &snapshot->nativeTypes->items[iter->nativeBaseTypeArrayIndex];
        if (iter->typeIndex == baseTypeIndex) { return true; }
    }
    
    return false;
}

bool MemorySnapshotCrawler::isPremitiveType(int32_t typeIndex)
{
    auto &mtypes = snapshot->managedTypeIndex;
    
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
    if (typeIndex == mtypes.system_Enum) {return true;}
    return false;
}

void MemorySnapshotCrawler::printPremitiveValue(address_t address, int32_t typeIndex, HeapMemoryReader *explicitReader)
{
    auto &mtypes = snapshot->managedTypeIndex;
    auto &memoryReader = explicitReader == nullptr ? *__memoryReader : *explicitReader;
    if (!memoryReader.isStatic()) {address += __vm->objectHeaderSize;}
    
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

void MemorySnapshotCrawler::dumpVRefChain(address_t address)
{
    auto candidates = findVObjectAtAddress(address);
    if (candidates == nullptr) {return;}
    for (auto index = candidates->begin(); index != candidates->end(); index++)
    {
        auto *mo = &managedObjects[*index];
        auto *type = &snapshot->typeDescriptions->items[mo->typeIndex];
        printf("0x%08llx:'%s'%d", mo->address, type->name.c_str(), type->typeIndex);
        
        while (type->isValueType)
        {
            auto &fromConnections = mo->fromConnections;
            if (fromConnections.size() == 0) {break;}
            
            auto fromIndex = fromConnections[0];
            auto &ec = connections[fromIndex];
            auto &ej = joints[ec.jointArrayIndex];
            
            mo = &managedObjects[ec.from];
            type = &snapshot->typeDescriptions->items[mo->typeIndex];
            
            printf("<-");
            if (ej.fieldSlotIndex >= 0)
            {
                auto &field = type->fields->items[ej.fieldSlotIndex];
                printf("{0x%08llx:'%s'%d}.%s", mo->address, type->name.c_str(), type->typeIndex, field.name.c_str());
            }
            else
            {
                assert(ej.elementArrayIndex >= 0);
                printf("{0x%08llx:'%s'%d}[%d]", mo->address, type->name.c_str(), type->typeIndex, ej.elementArrayIndex);
            }
        }
        printf("\n");
    }
}

EntityConnection* MemorySnapshotCrawler::getMRefNode(ManagedObject *mo, int32_t depth)
{
    EntityConnection *relation = nullptr;
    
    auto target = mo;
    while (target != nullptr && depth-- > 0)
    {
        auto &fromConnections = target->fromConnections;
        if (fromConnections.size() == 0) {break;}
        
        auto iter = fromConnections.begin();
        while (relation == nullptr || relation->fromKind == CK_link || relation->fromKind == CK_gcHandle)
        {
            if (iter == fromConnections.end()) {break;}
            relation = &connections[*iter++];
        }
        
        if (relation == nullptr || relation->fromKind == CK_link || relation->fromKind == CK_gcHandle || relation->fromKind == CK_static) {break;}
        
        auto mindex = relation->from;
        if (mindex <= -1 || mindex >= managedObjects.size()) {break;}
        target = &managedObjects[mindex];
    }
    
    return relation;
}

void MemorySnapshotCrawler::dumpMRefChain(address_t address, bool includeCircular, int32_t route, int32_t depth)
{
    auto objectIndex = findMObjectAtAddress(address);
    if (objectIndex == -1) {return;}
    
    auto *mo = &managedObjects[objectIndex];
    auto fullChains = iterateMRefChain(mo, route, depth, vector<int32_t>(), set<int64_t>());
    for (auto c = fullChains.begin(); c != fullChains.end(); c++)
    {
        auto &chain = *c;
        auto number = 0;
        for (auto n = chain.rbegin(); n != chain.rend(); n++)
        {
            auto index = *n;
            bool interupted = false;
            if (index < 0)
            {
                switch (index)
                {
                    case -1:
                        if (!includeCircular) { interupted = true; } else { printf("∞"); continue; } // circular
                        break;
                    case -2:
                        printf("*"); // more and interrupted
                        continue;
                    case -3:
                        printf("~"); // ignore in tracking mode
                        continue;
                }
            }
            
            if (interupted) {break;}
            
            auto &ec = connections[index];
            auto &node = managedObjects[ec.to];
            auto &joint = joints[ec.jointArrayIndex];
            auto &type = snapshot->typeDescriptions->items[node.typeIndex];
            
            if (ec.fromKind == CK_gcHandle)
            {
                printf("<GCHandle>::%s 0x%08llx\n", type.name.c_str(), node.address);
            }
            else if (ec.fromKind == CK_link)
            {
                auto &appending = snapshot->nativeAppendingCollection.appendings[joint.linkArrayIndex];
                auto &no = snapshot->nativeObjects->items[appending.link.nativeArrayIndex];
                auto &nt = snapshot->nativeTypes->items[no.nativeTypeArrayIndex];
                printf("<LINK>::0x%08llx '%s' %s <=> %s 0x%08llx\n", appending.link.nativeAddress, no.name.c_str(), nt.name.c_str(), type.name.c_str(), node.address);
            }
            else
            {
                auto &hookType = snapshot->typeDescriptions->items[joint.hookTypeIndex];
                if (number > 0) {printf("    .");}
                if (joint.fieldSlotIndex >= 0)
                {
                    auto &field = hookType.fields->items[joint.fieldSlotIndex];
                    if (ec.fromKind == CK_static)
                    {
                        auto &hookType = snapshot->typeDescriptions->items[joint.hookTypeIndex];
                        printf("<Static>::%s::", hookType.name.c_str());
                    }
                    
                    printf("{%s:%s} 0x%08llx\n", field.name.c_str(), type.name.c_str(), node.address);
                }
                else
                {
                    assert(joint.elementArrayIndex >= 0);
                    auto &elementType = snapshot->typeDescriptions->items[joint.fieldTypeIndex];
                    printf("{[%d]:%s} 0x%08llx\n", joint.elementArrayIndex, elementType.name.c_str(), node.address);
                }
            }
            
            ++number;
        }
    }
}

vector<vector<int32_t>> MemorySnapshotCrawler::iterateMRefChain(ManagedObject *mo, int32_t routeMaximum, int32_t depthMaximum,
                                                                vector<int32_t> chain, set<int64_t> antiCircular, int64_t __iter_capacity, int32_t __iter_depth)
{
    vector<vector<int32_t>> result;
    if (mo->fromConnections.size() > 0)
    {
        set<int64_t> unique;
        for (auto i = 0; i < mo->fromConnections.size(); i++)
        {
            if (routeMaximum > 0 && unique.size() >= routeMaximum) {break;}
            auto ci = mo->fromConnections[i];
            auto &ec = connections[ci];
            auto fromIndex = ec.from;
            
            vector<int32_t> __chain(chain);
            __chain.push_back(ci);
            
            if (ec.fromKind == CK_static || ec.fromKind == CK_gcHandle || ec.fromKind == CK_link || fromIndex < 0)
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
                if ((__iter_capacity * depthCapacity >= REF_ITERATE_CAPACITY && routeMaximum <= 0) || (routeMaximum > 1 && __iter_depth >= REF_ITERATE_DEPTH))
                {
                    __chain.push_back(-2); // interruptted signal
                    result.push_back(__chain);
                    continue;
                }
                
                if (depthMaximum > 0 && depthMaximum <= __iter_depth + 1)
                {
                    if (depthCapacity > 0) {__chain.push_back(-2);}
                    result.push_back(__chain);
                    continue;
                }
                
                auto branches = iterateMRefChain(fromObject, routeMaximum, depthMaximum, __chain, __antiCircular, __iter_capacity * depthCapacity, __iter_depth + 1);
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

void MemorySnapshotCrawler::dumpNRefChain(address_t address, bool includeCircular, int32_t route, int32_t depth)
{
    auto objectIndex = findNObjectAtAddress(address);
    if (objectIndex == -1) {return;}
    
    auto *no = &snapshot->nativeObjects->items[objectIndex];
    auto &nativeConnections = *snapshot->connections;
    auto fullChains = iterateNRefChain(no, route, depth, vector<int32_t>(), set<int64_t>());
    for (auto c = fullChains.begin(); c != fullChains.end(); c++)
    {
        auto &chain = *c;
        auto number = 0;
        for (auto n = chain.rbegin(); n != chain.rend(); n++)
        {
            auto index = *n;
            bool interupted = false;
            if (index < 0)
            {
                switch (index)
                {
                    case -1:
                        if (!includeCircular) { interupted = true; } else { printf("∞"); continue; } // circular
                        break;
                    case -2:
                        printf("*"); // more and interrupted
                        continue;
                    case -3:
                        printf("~"); // ignore in tracking mode
                        continue;
                }
            }
            
            if (interupted) {break;}
            
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
                    auto &node = snapshot->nativeObjects->items[nc.from];
                    auto &type = snapshot->nativeTypes->items[node.nativeTypeArrayIndex];
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
                    printf("{%s:0x%08llx:'%s'}.", type.name.c_str(), node.nativeObjectAddress, node.name.c_str());
                }
            }
            auto &node = snapshot->nativeObjects->items[nc.to];
            auto &type = snapshot->nativeTypes->items[node.nativeTypeArrayIndex];
            printf("{%s:0x%08llx:'%s'}.", type.name.c_str(), node.nativeObjectAddress, node.name.c_str());
            ++number;
        }
        if (number != 0){printf("\b \n");}
    }
}

vector<vector<int32_t>> MemorySnapshotCrawler::iterateNRefChain(PackedNativeUnityEngineObject *no, int32_t routeMaximum, int32_t depthMaximum,
                                                                vector<int32_t> chain, set<int64_t> antiCircular, int64_t __iter_capacity, int32_t __iter_depth)
{
    vector<vector<int32_t>> result;
    if (no->fromConnections.size() > 0)
    {
        set<int64_t> unique;
        for (auto i = 0; i < no->fromConnections.size(); i++)
        {
            if (routeMaximum > 0 && unique.size() >= routeMaximum) {break;}
            auto ci = no->fromConnections[i];
            auto &nc = snapshot->connections->items[ci];
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
                
                auto *fromObject = &snapshot->nativeObjects->items[fromIndex];
                auto depthCapacity = fromObject->fromConnections.size();
                if ((__iter_capacity * depthCapacity >= REF_ITERATE_CAPACITY && routeMaximum <= 0) || (routeMaximum > 1 && __iter_depth >= REF_ITERATE_DEPTH))
                {
                    __chain.push_back(-2); // interruptted signal
                    result.push_back(__chain);
                    continue;
                }
                
                if (depthMaximum > 0 && depthMaximum <= __iter_depth + 1)
                {
                    if (depthCapacity > 0) {__chain.push_back(-2);}
                    result.push_back(__chain);
                    continue;
                }
                
                auto branches = iterateNRefChain(fromObject, routeMaximum, depthMaximum, __chain, __antiCircular, __iter_capacity * depthCapacity, __iter_depth + 1);
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
    if (nc.from >= 0 && nc.fromKind == CK_native)
    {
        auto &no = snapshot->nativeObjects->items[nc.from];
        no.toConnections.push_back(nc.connectionArrayIndex);
    }
    
    if (nc.to >= 0 && nc.toKind == CK_native)
    {
        auto &no = snapshot->nativeObjects->items[nc.to];
        no.fromConnections.push_back(nc.connectionArrayIndex);
    }
}

void MemorySnapshotCrawler::tryAcceptConnection(EntityConnection &ec)
{
    if (ec.fromKind == CK_managed && ec.from >= 0)
    {
        auto &mo = managedObjects[ec.from];
        mo.toConnections.push_back(ec.connectionArrayIndex);
    }
    
    if (ec.toKind == CK_managed && ec.to >= 0)
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
        Array<TypeDescription> &typeDescriptions = *snapshot->typeDescriptions;
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
    auto typeIndex = findTypeAtTypeAddress(address); // il2cpp
    if (typeIndex != -1) {return typeIndex;}
    // MonoObject->vtable->kclass
    auto vtable = __memoryReader->readPointer(address);
    if (vtable == 0) {return -1;}
    auto klass = __memoryReader->readPointer(vtable);
    if (klass != 0)
    {
        return findTypeAtTypeAddress(klass);
    }
    return findTypeAtTypeAddress(vtable);
}

void MemorySnapshotCrawler::dumpRepeatedObjects(int32_t typeIndex, int32_t condition)
{
    auto &type = snapshot->typeDescriptions->items[typeIndex];
    
    map<size_t, vector<int32_t>> stats;
    for (auto i = 0; i < managedObjects.size(); i++)
    {
        auto &mo = managedObjects[i];
        if (mo.typeIndex == typeIndex)
        {
            size_t hash = 0;
            if (type.isValueType)
            {
                auto *data = __memoryReader->readMemory(mo.address + __vm->objectHeaderSize);
                 hash = __hash.get(data, mo.size);
            }
            else
            {
                auto *data = __memoryReader->readMemory(mo.address);
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
    
    printf("%s typeIndex=%d instanceCount=%d instanceMemory=%d\n", type.name.c_str(), typeIndex, type.instanceCount, type.instanceMemory);
    for (auto iter = stats.begin(); iter != stats.end(); iter++)
    {
        auto &children = iter->second;
        if ((condition > 0 && children.size() < condition) || children.size() < 2) {continue;}
        
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
                  if (ma != mb) { return ma < mb; }
                  auto &ca = stats.at(a);
                  auto &cb = stats.at(b);
                  return ca.size() > cb.size();
              });
    auto isString = type.typeIndex == snapshot->managedTypeIndex.system_String;
    for (auto iter = target.begin(); iter != target.end(); iter++)
    {
        auto hash = *iter;
        auto &children = stats.at(hash);
        printf("\e[36m%s #%-2d", comma(memory.at(hash)).c_str(), (int32_t)children.size());
        
        bool extraComplate = false;
        for (auto n= children.begin(); n != children.end(); n++)
        {
            auto &mo = managedObjects[*n];
            auto size = 0;
            if (!extraComplate && isString) {printf(" \e[32m'%s'", getUTFString(mo.address, size, true).c_str());}
            printf(" \e[33m0x%08llx", mo.address);
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
                auto &no = snapshot->nativeObjects->items[mo.nativeObjectIndex];
                __managedNativeAddressMap.insert(pair<address_t, int32_t>(no.nativeObjectAddress, mo.managedObjectIndex));
            }
        }
    }
    
    auto iter = __managedNativeAddressMap.find(address);
    if (iter == __managedNativeAddressMap.end())
    {
        auto &link = snapshot->nativeAppendingCollection.nmAddressMap;
        auto match = link.find(address);
        return match != link.end()? match->second : 0;
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
        auto &link = snapshot->nativeAppendingCollection.mnAddressMap;
        auto match = link.find(address);
        return match != link.end()? match->second : 0;
    }
    else
    {
        return snapshot->nativeObjects->items[iter->second].nativeObjectAddress;
    }
}

vector<int32_t> *MemorySnapshotCrawler::findVObjectAtAddress(address_t address)
{
    if (address < 0xFFFF){return nullptr;}
    if (__valueAddressMap.size() == 0)
    {
        for (auto i = 0; i < managedObjects.size(); i++)
        {
            auto &mo = managedObjects[i];
            auto &type = snapshot->typeDescriptions->items[mo.typeIndex];
            
            if (type.isValueType)
            {
                auto iter = __valueAddressMap.find(mo.address);
                if (iter == __valueAddressMap.end())
                {
                    iter = __valueAddressMap.insert(pair<address_t, vector<int32_t>>(mo.address, {})).first;
                }
                
                iter->second.push_back(mo.managedObjectIndex);
            }
        }
    }
    
    auto iter = __valueAddressMap.find(address);
    return iter != __valueAddressMap.end() ? &iter->second : nullptr;
}

int32_t MemorySnapshotCrawler::findMObjectAtAddress(address_t address)
{
    if (address == 0){return -1;}
    if (__managedObjectAddressMap.size() == 0)
    {
        for (auto i = 0; i < managedObjects.size(); i++)
        {
            auto &mo = managedObjects[i];
            auto &type = snapshot->typeDescriptions->items[mo.typeIndex];
            if (!type.isValueType)
            {
                __managedObjectAddressMap.insert(pair<address_t, int32_t>(mo.address, mo.managedObjectIndex));
            }
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
        auto &nativeObjects = *snapshot->nativeObjects;
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
    
    auto &type = snapshot->typeDescriptions->items[mo.typeIndex];
    mo.isValueType = type.isValueType;
    if (mo.isValueType || type.isArray || !type.isUnityEngineObjectType) {return;}
    
    auto nativeAddress = __memoryReader->readPointer(mo.address + snapshot->cached_ptr->offset);
    if (nativeAddress == 0){return;}
    
    auto nativeObjectIndex = findNObjectAtAddress(nativeAddress);
    if (nativeObjectIndex == -1){return;}
    
    // connect managed/native objects
    auto &no = snapshot->nativeObjects->items[nativeObjectIndex];
    mo.nativeObjectIndex = nativeObjectIndex;
    mo.nativeSize = no.size;
    no.managedObjectArrayIndex = mo.managedObjectIndex;
    
    // connect managed/native types
    auto &nativeType = snapshot->nativeTypes->items[no.nativeTypeArrayIndex];
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
    
    auto elementType = &snapshot->typeDescriptions->items[type.baseOrElementTypeIndex];
    if (!isCrawlable(*elementType)) {return false;}
    
    auto successCount = 0;
    address_t elementAddress = 0;
    auto elementCount = memoryReader.readArrayLength(address, type);
    if (elementCount >= 1E+8) // 100 million
    {
        printf("\e[31m[E] huge array[size=%u] at *0x%08llx\n\e[0m", elementCount, address);
        abort();
    }
    for (auto i = 0; i < elementCount; i++)
    {
        if (elementType->isValueType)
        {
            elementAddress = address + __vm->arrayHeaderSize + i *elementType->size - __vm->objectHeaderSize;
        }
        else
        {
            auto ptrAddress = address + __vm->arrayHeaderSize + i * __vm->pointerSize;
            elementAddress = memoryReader.readPointer(ptrAddress);
            if (elementAddress == 0) {continue;}
            
            auto typeIndex = findTypeOfAddress(elementAddress);
            if (typeIndex >= 0)
            {
                auto derivedType = &snapshot->typeDescriptions->items[typeIndex];
                if (elementType->baseOrElementTypeIndex == -1 || deriveFromMType(*derivedType, elementType->typeIndex)) // sometimes get wrong type from object type pointer
                {
                    elementType = derivedType;
                }
            }
        }
        
        auto &elementJoint = joints.add();
        elementJoint.jointArrayIndex = joints.size() - 1;
        elementJoint.isConnected = false;
        
        // set field hook info
        elementJoint.hookObjectAddress = address;
        elementJoint.hookObjectIndex = joint.managedArrayIndex;
        elementJoint.hookTypeIndex = joint.fieldTypeIndex;
        
        // set field info
        elementJoint.fieldAddress = elementAddress;
        elementJoint.fieldTypeIndex = elementType->typeIndex;
        
        // set element info
        elementJoint.elementArrayIndex = i;
        
        auto success = crawlManagedEntryAddress(elementAddress, elementType, memoryReader, elementJoint, true, depth + 1);
        if (success) { successCount += 1; }
    }
    
    return successCount != 0;
}

bool MemorySnapshotCrawler::crawlManagedEntryAddress(address_t address, TypeDescription *type, HeapMemoryReader &memoryReader, EntityJoint &joint, bool isActualType, int32_t depth)
{
    auto isStaticCrawling = memoryReader.isStatic();
    if (address < 0 || (!isStaticCrawling && address == 0)){return false;}
    if (depth >= 1024) {return false;}
    
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
    
    auto &entryType = snapshot->typeDescriptions->items[typeIndex];
    
    ManagedObject *mo;
    auto iter = __crawlingVisit.find(address);
    if (entryType.isValueType || iter == __crawlingVisit.end())
    {
        mo = &createManagedObject(address, typeIndex);
        mo->size = memoryReader.readObjectSize(mo->address, entryType);
        mo->isValueType = entryType.isValueType;
        assert(typeIndex < snapshot->typeDescriptions->size);
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
    else if (joint.linkArrayIndex >= 0)
    {
        ec.fromKind = CK_link;
        ec.from = joint.linkArrayIndex;
    }
    else
    {
        ec.fromKind = CK_managed;
        ec.from = joint.hookObjectIndex;
    }
    
    ec.toKind = CK_managed;
    ec.to = mo->managedObjectIndex;
    ec.jointArrayIndex = joint.jointArrayIndex;
    joint.managedArrayIndex = mo->managedObjectIndex;
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
            
            auto *fieldType = &snapshot->typeDescriptions->items[field.typeIndex];
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
                if (fieldAddress == 0) {continue;}
                
                auto fieldTypeIndex = findTypeOfAddress(fieldAddress);
                if (fieldTypeIndex != -1)
                {
                    auto derivedType = &snapshot->typeDescriptions->items[fieldTypeIndex];
                    if (fieldType->baseOrElementTypeIndex == -1 || deriveFromMType(*derivedType, fieldType->typeIndex)) // sometimes get wrong type from object type pointer
                    {
                        fieldType = derivedType;
                    }
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
            iterType = &snapshot->typeDescriptions->items[iterType->baseOrElementTypeIndex];
        }
    }
    
    return successCount != 0;
}

int32_t MemorySnapshotCrawler::getReferencedMemoryOf(address_t address, TypeDescription *type, std::set<address_t> &antiCircular, bool verbose)
{
    TypeDescription *mt = nullptr;
    ManagedObject *mo = nullptr;
    if (type == nullptr)
    {
        auto index = findMObjectAtAddress(address);
        if (index >= 0)
        {
            mo = &managedObjects[index];
            mt = &snapshot->typeDescriptions->items[mo->typeIndex];
        }
    }
    else
    {
        mt = type;
        if (!type->isValueType)
        {
            auto index = findMObjectAtAddress(address);
            if (index >= 0)
            {
                mo = &managedObjects[index];
                mt = &snapshot->typeDescriptions->items[mo->typeIndex];
            }
        }
    }
    
    if (mt != nullptr)
    {
        if (!mt->isValueType)
        {
            if (mt->typeIndex == snapshot->managedTypeIndex.system_Object)
            {
                return mt->size;
            }
            
            assert(mo != nullptr);
            if (antiCircular.find(address) != antiCircular.end()) { return 0; }
            antiCircular.insert(address);
        }
        
        auto total = mt->isValueType? 0 : mo->size;
        if (mt->isArray)
        {
            auto &et = snapshot->typeDescriptions->items[mt->baseOrElementTypeIndex];
            auto elementCount = __memoryReader->readArrayLength(address, et);
            if (elementCount >= 1E+8) { return 0; }
            
            if (et.isValueType)
            {
                if (!isPremitiveType(et.typeIndex))
                {
                    for (auto i = 0; i < elementCount; i++)
                    {
                        auto valueAddress = address + __vm->arrayHeaderSize + i * et.size - __vm->objectHeaderSize;
                        total += getReferencedMemoryOf(valueAddress, &et, antiCircular, false);
                    }
                }
            }
            else
            {
                for (auto i = 0; i < elementCount; i++)
                {
                    auto ptrAddress = address + __vm->arrayHeaderSize + i * __vm->pointerSize;
                    auto elementAddress = __memoryReader->readPointer(ptrAddress);
                    if (elementAddress == 0) {continue;}
                    total += getReferencedMemoryOf(elementAddress, &et, antiCircular, false);
                }
            }
        }
        else
        {
            auto needCrawling = mt->typeIndex != snapshot->managedTypeIndex.system_String;
            if (needCrawling)
            {
                auto itype = mt;
                while (itype != nullptr)
                {
                    for (auto i = 0; i < itype->fields->size; i++)
                    {
                        auto &field = itype->fields->items[i];
                        
                        if (field.isStatic) {continue;}
                        auto &ft = snapshot->typeDescriptions->items[field.typeIndex];
                        if (isPremitiveType(ft.typeIndex)) {continue;}
                        if (ft.isValueType)
                        {
                            auto slotAddress = address + field.offset - __vm->objectHeaderSize;
                            total += getReferencedMemoryOf(slotAddress, &ft, antiCircular, false);
                        }
                        else
                        {
                            auto slotAddress = address + field.offset;
                            auto fieldAddress = __memoryReader->readPointer(slotAddress);
                            if (fieldAddress == 0) {continue;}
                            total += getReferencedMemoryOf(fieldAddress, &ft, antiCircular, false);
                        }
                    }
                    
                    if (itype->baseOrElementTypeIndex < 0 || itype->isValueType) { itype = nullptr ;}
                    else { itype = &snapshot->typeDescriptions->items[itype->baseOrElementTypeIndex]; }
                }
            }
        }
        
        if (verbose)
        {
            printf("0x%llx %s type='%s'%d\n", address, comma(total).c_str(), mt->name.c_str(), mt->typeIndex);
        }
        
        return total;
    }
    return 0;
}

void MemorySnapshotCrawler::dumpGCHandles()
{
    vector<int32_t> handleTargets;
    for (auto i = 0; i < snapshot->gcHandles->size; i++)
    {
        auto &handle = snapshot->gcHandles->items[i];
        auto index = findMObjectAtAddress(handle.target);
        assert(index >= 0);
        handleTargets.push_back(index);
    }
    
    std::sort(handleTargets.begin(), handleTargets.end(), [&](int32_t a, int32_t b)
              {
                  auto &aObject = managedObjects[a];
                  auto &bObject = managedObjects[b];
                  if (aObject.typeIndex != bObject.typeIndex)
                  {
                      return aObject.typeIndex < bObject.typeIndex;
                  }
                  return aObject.address < bObject.address;
              });
    
    auto digit = handleTargets.size() == 0? 1 : (int)ceil(log10(handleTargets.size()));
    
    char format[32];
    memset(format, 0, sizeof(format));
    sprintf(format, "[%%%dd/%d] ", digit, (int32_t)handleTargets.size());
    
    auto num = 0;
    for (auto i = handleTargets.begin(); i != handleTargets.end(); i++)
    {
        auto index = *i;
        auto &mo = managedObjects[index];
        
        printf(format, ++num);
        auto &type = snapshot->typeDescriptions->items[mo.typeIndex];
        printf("0x%08llx type='%s'%d size=%d assembly='%s'", mo.address, type.name.c_str(), type.typeIndex, mo.size, type.assembly.c_str());
        if (mo.nativeObjectIndex >= 0)
        {
            auto &no = snapshot->nativeObjects->items[mo.nativeObjectIndex];
            printf(" N[0x%08llx size=%d]", no.nativeObjectAddress, no.size);
        }
        
        std::cout << std::endl;
    }
}

void MemorySnapshotCrawler::inspectVObject(address_t address)
{
    auto candidates = findVObjectAtAddress(address);
    if (candidates == nullptr) {return;}
    
    for (auto index = candidates->begin(); index != candidates->end(); index++)
    {
        auto &mo = managedObjects[*index];
        auto &type = snapshot->typeDescriptions->items[mo.typeIndex];
        
        dumpVObjectHierarchy(address, type, "");
    }
}

void MemorySnapshotCrawler::inspectMObject(address_t address, int32_t depth)
{
    auto index = findMObjectAtAddress(address);
    if (index == -1) {return;}
    
    auto &mo = managedObjects[index];
    auto &type = snapshot->typeDescriptions->items[mo.typeIndex];
    dumpMObjectHierarchy(address, &type, set<int64_t>(), true, depth, "");
}

void MemorySnapshotCrawler::inspectNObject(address_t address, int32_t depth)
{
    auto index = findNObjectAtAddress(address);
    if (index <= 0) {return;}
    
    dumpNObjectHierarchy(&snapshot->nativeObjects->items[index], set<int64_t>(), depth, "");
}

void MemorySnapshotCrawler::inspectMType(int32_t typeIndex)
{
    if (typeIndex < 0 || typeIndex >= snapshot->typeDescriptions->size) {return;}
    
    auto &type = snapshot->typeDescriptions->items[typeIndex];
    printf("\e[1m%s\e[0m\e[36m typeIndex=%d size=%d", type.name.c_str(), type.typeIndex, type.size);
    if (type.baseOrElementTypeIndex >= 0) {printf(" baseOrElementType=%d", type.baseOrElementTypeIndex);}
    if (type.isValueType) {printf(" isValueType=%s", type.isValueType ? "true" : "false");}
    if (type.isArray) {printf(" isArray=%s arrayRank=%d", type.isArray ? "true" : "false", type.arrayRank);}
    if (type.staticFieldBytes != nullptr) {printf(" staticFieldBytes=%d", type.staticFieldBytes->size);}
    printf(" assembly='%s'", type.assembly.c_str());
    if (type.instanceCount > 0) {printf(" %s #%d", comma(type.instanceMemory).c_str(), type.instanceCount);}
    if (type.nativeTypeArrayIndex >= 0)
    {
        auto &nt = snapshot->nativeTypes->items[type.nativeTypeArrayIndex];
        printf(" N[%s #%d]", comma(nt.instanceMemory).c_str(), nt.instanceCount);
    }
    printf("\n");
    if (!type.isArray)
    {
        for (auto i = 0; i < type.fields->size; i++)
        {
            auto &field = type.fields->items[i];
            printf("    isStatic=%s name='%s' offset=%d typeIndex=%d\n", field.isStatic ? "true" : "false", field.name.c_str(), field.offset, field.typeIndex);
        }
        
        if (type.baseOrElementTypeIndex >= 0)
        {
            inspectMType(type.baseOrElementTypeIndex);
        }
    }
}

void MemorySnapshotCrawler::inspectNType(int32_t typeIndex)
{
    if (typeIndex < 0 || typeIndex >= snapshot->nativeTypes->size) {return;}
    
    auto &type = snapshot->nativeTypes->items[typeIndex];
    printf("name='%s'%d", type.name.c_str(), type.typeIndex);
    if (type.nativeBaseTypeArrayIndex >= 0)
    {
        auto &baseType = snapshot->nativeTypes->items[type.nativeBaseTypeArrayIndex];
        printf(" nativeBaseType='%s'%d", baseType.name.c_str(), baseType.typeIndex);
    }
    printf(" %s #%d", comma(type.instanceMemory).c_str(), type.instanceCount);
    if (type.managedTypeArrayIndex >= 0)
    {
        auto &mt = snapshot->typeDescriptions->items[type.managedTypeArrayIndex];
        printf(" M[0x%08llx typeIndex=%d %s #%d]", mt.typeInfoAddress, mt.typeIndex, comma(mt.instanceMemory).c_str(), mt.instanceCount);
    }
    printf("\n");
}

void MemorySnapshotCrawler::findMObject(address_t address)
{
    auto index = findMObjectAtAddress(address);
    if (index >= 0)
    {
        auto &mo = managedObjects[index];
        auto &type = snapshot->typeDescriptions->items[mo.typeIndex];
        printf("0x%08llx type='%s'%d size=%d assembly='%s'", address, type.name.c_str(), type.typeIndex, mo.size, type.assembly.c_str());
        if (mo.nativeObjectIndex >= 0)
        {
//            auto __address = findNObjectOfMObject(address);
//            auto __index = findNObjectAtAddress(__address);
//            assert(__index >= 0);
            
            auto &no = snapshot->nativeObjects->items[mo.nativeObjectIndex];
            printf(" N[0x%08llx size=%d]", no.nativeObjectAddress, no.size);
        }
        std::cout << std::endl;
    }
    else
    {
        printf("not found managed object at address[0x%08llx]\n", address);
    }
}

void MemorySnapshotCrawler::findNObject(address_t address)
{
    auto index = findNObjectAtAddress(address);
    if (index >= 0)
    {
        auto &no = snapshot->nativeObjects->items[index];
        auto &nt = snapshot->nativeTypes->items[no.nativeTypeArrayIndex];
        printf("0x%08llx name='%s' type='%s'%d size=%d", no.nativeObjectAddress, no.name.c_str(), nt.name.c_str(), nt.typeIndex, no.size);
        if (no.managedObjectArrayIndex >= 0)
        {
            auto &mo = managedObjects[no.managedObjectArrayIndex];
            auto &mt = snapshot->typeDescriptions->items[mo.typeIndex];
            printf(" M[0x%08llx type='%s'%d size=%d]", mo.address, mt.name.c_str(), mt.typeIndex, mo.size);
        }
        printf("\n");
    }
    else
    {
        printf("not found native object at address[0x%08llx]\n", address);
    }
}

void MemorySnapshotCrawler::printByteArray(const char *data, int32_t size)
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

void MemorySnapshotCrawler::dumpNObjectHierarchy(PackedNativeUnityEngineObject *no, set<int64_t> antiCircular, int32_t limit, const char *indent, int32_t __iter_depth, int32_t __iter_capacity)
{
    auto __size = strlen(indent);
    char __indent[__size + 2*3 + 1]; // indent + 2×tabulator + \0
    memset(__indent, 0, sizeof(__indent));
    memcpy(__indent, indent, __size);
    char *tabular = __indent + __size;
    memcpy(tabular + 3, "─", 3);
    
    if (__iter_depth == 0)
    {
        auto &type = snapshot->nativeTypes->items[no->nativeTypeArrayIndex];
        printf("%s'%s':%s 0x%08llx\n", indent, no->name.c_str(), type.name.c_str(), no->nativeObjectAddress);
    }
    
    auto toCount = (int32_t)no->toConnections.size();
    for (auto i = 0; i < toCount; i++)
    {
        auto closed = i + 1 == toCount;
        auto &nc = snapshot->connections->items[no->toConnections[i]];
        auto &no = snapshot->nativeObjects->items[nc.to];
        auto &nt = snapshot->nativeTypes->items[no.nativeTypeArrayIndex];
        
        closed ? memcpy(tabular, "└", 3) : memcpy(tabular, "├", 3);
        printf("%s'%s':%s 0x%08llx=%d\n", __indent, no.name.c_str(), nt.name.c_str(), no.nativeObjectAddress, no.size);
        
        if (antiCircular.find(no.nativeObjectAddress) == antiCircular.end())
        {
            decltype(antiCircular) __antiCircular(antiCircular);
            __antiCircular.insert(no.nativeObjectAddress);
            if (limit > 0 && __iter_depth + 1 >= limit) {continue;}
            if (closed)
            {
                char __nest_indent[__size + 1 + 2 + 1]; // indent + space + 2×space + \0
                memcpy(__nest_indent, indent, __size);
                memset(__nest_indent + __size, '\x20', 3);
                memset(__nest_indent + __size + 3, 0, 1);
                dumpNObjectHierarchy(&no, __antiCircular, limit, __nest_indent, __iter_depth + 1, __iter_capacity * toCount);
            }
            else
            {
                char __nest_indent[__size + 3 + 2 + 1]; // indent + tabulator + 2×space + \0
                char *iter = __nest_indent + __size;
                memcpy(__nest_indent, indent, __size);
                memcpy(iter, "│", 3);
                memset(iter + 3, '\x20', 2);
                memset(iter + 5, 0, 1);
                dumpNObjectHierarchy(&no, __antiCircular, limit, __nest_indent, __iter_depth + 1, __iter_capacity * toCount);
            }
        }
    }
}

void MemorySnapshotCrawler::dumpVObjectHierarchy(address_t address, TypeDescription &type, const char *indent, int32_t __iter_depth)
{
    auto __size = strlen(indent);
    char __indent[__size + 2*3 + 1]; // indent + 2×tabulator + \0
    memset(__indent, 0, sizeof(__indent));
    memcpy(__indent, indent, __size);
    char *tabular = __indent + __size;
    memcpy(tabular + 3, "─", 3);
    if (__iter_depth == 0)
    {
        printf("0x%08llx type='%s'%d\n", address, type.name.c_str(), type.typeIndex);
    }
    
    vector<FieldDescription *> typeMembers;
    for (auto i = 0; i < type.fields->size; i++)
    {
        auto &field = type.fields->items[i];
        if (!field.isStatic) { typeMembers.push_back(&field); }
    }
    
    auto fieldCount = typeMembers.size();
    for (auto i = 0; i < fieldCount; i++)
    {
        auto code = 0;
        auto closed = i + 1 == fieldCount;
        auto &field = *typeMembers[i];
        auto __is_premitive = isPremitiveType(field.typeIndex);
        
        address_t fieldAddress = 0;
        auto *fieldType = &snapshot->typeDescriptions->items[field.typeIndex];
        if (!fieldType->isValueType)
        {
            fieldAddress = __memoryReader->readPointer(address + field.offset);
            if (field.typeIndex == snapshot->managedTypeIndex.system_String)
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
                fieldType = &snapshot->typeDescriptions->items[typeIndex];
            }
        }
        else
        {
            fieldAddress = address + field.offset - __vm->objectHeaderSize;
            if (field.typeIndex == type.typeIndex || __is_premitive)
            {
                code = 3;
            }
        }
        
        closed ? memcpy(tabular, "└", 3) : memcpy(tabular, "├", 3);
        const char *fieldTypeName = fieldType->name.c_str();
        const char *fieldName = field.name.c_str();
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
                printPremitiveValue(fieldAddress, fieldType->typeIndex);
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
        
        if (fieldType->isValueType && !__is_premitive)
        {
            if (closed)
            {
                char __nest_indent[__size + 1 + 2 + 1]; // indent + space + 2×space + \0
                memcpy(__nest_indent, __indent, __size);
                memset(__nest_indent + __size, '\x20', 3);
                memset(__nest_indent + __size + 3, 0, 1);
                dumpVObjectHierarchy(fieldAddress, *fieldType, __nest_indent, __iter_depth + 1);
            }
            else
            {
                char __nest_indent[__size + 3 + 2 + 1]; // indent + tabulator + 2×space + \0
                char *iter = __nest_indent + __size;
                memcpy(__nest_indent, __indent, __size);
                memcpy(iter, "│", 3);
                memset(iter + 3, '\x20', 2);
                memset(iter + 5, 0, 1);
                dumpVObjectHierarchy(fieldAddress, *fieldType, __nest_indent, __iter_depth + 1);
            }
        }
    }
}

void MemorySnapshotCrawler::listAllStatics()
{
    vector<int32_t> indice;
    auto &typeDescriptions = snapshot->typeDescriptions->items;
    for (auto i = 0; i < snapshot->typeDescriptions->size; i++)
    {
        auto &type = typeDescriptions[i];
        if (type.staticFieldBytes != nullptr && type.staticFieldBytes->size > 0)
        {
            indice.push_back(i);
        }
    }
    
    std::sort(indice.begin(), indice.end(), [&](int32_t a, int32_t b)
              {
                  auto &atype = typeDescriptions[a];
                  auto &btype = typeDescriptions[b];
                  if (atype.staticFieldBytes->size != btype.staticFieldBytes->size)
                  {
                      return atype.staticFieldBytes->size > btype.staticFieldBytes->size;
                  }
                  return a < b;
              });
    for (auto i = indice.begin(); i != indice.end(); i++)
    {
        auto &type = typeDescriptions[*i];
        auto staticCount = 0;
        for (auto n = 0; n < type.fields->size; n++)
        {
            auto &field = type.fields->items[n];
            if (field.isStatic)
            {
                ++staticCount;
            }
        }
        
        if (staticCount == 0) {continue;}
        printf("\e[36m%5d #%-3d \e[32m%s \e[33m%s \e[37m*%d\n", type.staticFieldBytes->size, staticCount, type.name.c_str(), type.assembly.c_str(), type.typeIndex);
    }
}

void MemorySnapshotCrawler::dumpStatic(int32_t typeIndex, bool verbose)
{
    char indent[2*3 + 1]; // indent + 2×tabulator + \0
    memset(indent, 0, sizeof(indent));
    memcpy(indent + 3, "─", 3);
    
    auto &type = snapshot->typeDescriptions->items[typeIndex];
    if (type.typeIndex != typeIndex) {return;}
    
    vector<int32_t> indice;
    for (auto i = 0; i < type.fields->size; i++)
    {
        auto &field = type.fields->items[i];
        if (field.isStatic) {indice.push_back(i);}
    }
    
    auto fieldCount = (int32_t)indice.size();
    if (type.staticFieldBytes == nullptr)
    {
        printf("%s %d #%d *%d\n", type.name.c_str(), 0, fieldCount, type.typeIndex);
        return;
    }
    
    printf("%s %d #%d *%d ", type.name.c_str(), type.staticFieldBytes->size, fieldCount, type.typeIndex);
    if (verbose && type.staticFieldBytes->size > 0)
    {
        printByteArray((const char *)type.staticFieldBytes->items, type.staticFieldBytes->size);
    }
    printf("\n");
    
    StaticMemoryReader reader(snapshot);
    reader.load(*type.staticFieldBytes);
    for (auto i = 0; i < fieldCount; i++)
    {
        auto &field = type.fields->items[indice[i]];
        auto closed = i + 1 == fieldCount;
        closed ? memcpy(indent, "└", 3) : memcpy(indent, "├", 3);
        
        const char *fieldName = field.name.c_str();
        auto fieldType = &snapshot->typeDescriptions->items[field.typeIndex];
        auto __is_premitive = isPremitiveType(field.typeIndex);
        
        if (fieldType->isValueType)
        {
            if (__is_premitive)
            {
                printf("%s%s:%s = ", indent, fieldName, fieldType->name.c_str());
                printPremitiveValue(field.offset, fieldType->typeIndex, &reader);
                printf("\n");
            }
            else if (*fieldType->name.rbegin() == '*' && fieldType->size == 8)
            {
                printf("%s%s:%s = 0x%08llx\n", indent, fieldName, fieldType->name.c_str(), reader.readPointer(field.offset));
            }
            else
            {
                printf("%s%s:%s\n", indent, fieldName, fieldType->name.c_str());
                dumpSObjectHierarchy(field.offset, *fieldType, reader, getNestIndent(indent, 0, closed).c_str());
            }
        }
        else
        {
            address_t fieldAddress = reader.readPointer(field.offset);
            if (fieldAddress == 0)
            {
                printf("%s%s:%s = NULL\n", indent, fieldName, fieldType->name.c_str());
            }
            else
            {
                auto typeIndex = findTypeOfAddress(fieldAddress);
                if (typeIndex >= 0)
                {
                    fieldType = &snapshot->typeDescriptions->items[typeIndex];
                }
                
                if (typeIndex == snapshot->managedTypeIndex.system_String)
                {
                    auto size = 0;
                    printf("%s%s:%s 0x%08llx = '%s'\n", indent, fieldName, fieldType->name.c_str(), fieldAddress, getUTFString(fieldAddress, size, true).c_str());
                }
                else
                {
                    printf("%s%s:%s = 0x%08llx\n", indent, fieldName, fieldType->name.c_str(), fieldAddress);
                }
            }
        }
    }
}

string MemorySnapshotCrawler::getNestIndent(const char *__indent, size_t __preindent_size, bool closed)
{
    if (closed)
    {
        char __nest_indent[__preindent_size + 1 + 2 + 1]; // indent + space + 2×space + \0
        if (__preindent_size > 0) {memcpy(__nest_indent, __indent, __preindent_size);}
        memset(__nest_indent + __preindent_size, '\x20', 3);
        memset(__nest_indent + __preindent_size + 3, 0, 1);
        return __nest_indent;
    }
    else
    {
        char __nest_indent[__preindent_size + 3 + 2 + 1]; // indent + tabulator + 2×space + \0
        char *iter = __nest_indent + __preindent_size;
        if (__preindent_size) {memcpy(__nest_indent, __indent, __preindent_size);}
        memcpy(iter, "│", 3);
        memset(iter + 3, '\x20', 2);
        memset(iter + 5, 0, 1);
        return __nest_indent;
    }
}

void MemorySnapshotCrawler::dumpSObjectHierarchy(address_t address, TypeDescription &type, StaticMemoryReader &memoryReader, const char *indent)
{
    auto __size = strlen(indent);
    char __indent[__size + 2*3 + 1]; // indent + 2×tabulator + \0
    memset(__indent, 0, sizeof(__indent));
    memcpy(__indent, indent, __size);
    char *tabular = __indent + __size;
    memcpy(tabular + 3, "─", 3);
    
    vector<int32_t> indice;
    for (auto i = 0; i < type.fields->size; i++)
    {
        auto &field = type.fields->items[i];
        if (!field.isStatic) {indice.push_back(i);}
    }
    
    auto fieldCount = indice.size();
    for (auto i = 0; i < fieldCount; i++)
    {
        auto closed = i + 1 == fieldCount;
        auto &field = type.fields->items[indice[i]];
        auto __is_premitive = isPremitiveType(field.typeIndex);
        
        auto fieldType = &snapshot->typeDescriptions->items[field.typeIndex];
        
        closed ? memcpy(tabular, "└", 3) : memcpy(tabular, "├", 3);
        const char *fieldName = field.name.c_str();
        if (fieldType->isValueType)
        {
            if (__is_premitive)
            {
                printf("%s%s:%s = ", __indent, fieldName, fieldType->name.c_str());
                printPremitiveValue(address + field.offset - __vm->objectHeaderSize, fieldType->typeIndex, &memoryReader);
                printf("\n");
            }
            else
            {
                printf("%s%s:%s\n", __indent, fieldName, fieldType->name.c_str());
                dumpSObjectHierarchy(address + field.offset - __vm->objectHeaderSize, *fieldType, memoryReader, getNestIndent(__indent, __size, closed).c_str());
            }
        }
        else
        {
            address_t fieldAddress = memoryReader.readPointer(address + field.offset);
            if (fieldAddress == 0)
            {
                printf("%s%s:%s = NULL\n", __indent, fieldName, fieldType->name.c_str());
            }
            else
            {
                auto typeIndex = findTypeOfAddress(fieldAddress);
                if (typeIndex >= 0)
                {
                    fieldType = &snapshot->typeDescriptions->items[typeIndex];
                }
                
                if (typeIndex == snapshot->managedTypeIndex.system_String)
                {
                    auto size = 0;
                    printf("%s%s:%s 0x%08llx = '%s'\n", __indent, fieldName, fieldType->name.c_str(), fieldAddress, getUTFString(fieldAddress, size, true).c_str());
                }
                else
                {
                    printf("%s%s:%s = 0x%08llx\n", __indent, fieldName, fieldType->name.c_str(), fieldAddress);
                }
            }
        }
    }
}

void MemorySnapshotCrawler::dumpMObjectHierarchy(address_t address, TypeDescription *type,
                                                 set<int64_t> antiCircular, bool isActualType, int32_t limit, const char *indent, int32_t __iter_depth)
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
        
        type = &snapshot->typeDescriptions->items[typeIndex];
    }
    
    auto &memoryReader = *__memoryReader;
    
    auto &entryType = *type;
    if (__iter_depth == 0)
    {
        printf("%s 0x%08llx\n", entryType.name.c_str(), address);
    }
    
    if (entryType.isArray)
    {
        auto *elementType = &snapshot->typeDescriptions->items[entryType.baseOrElementTypeIndex];
        auto __is_premitive = isPremitiveType(elementType->typeIndex);
        auto __is_string = elementType->typeIndex == snapshot->managedTypeIndex.system_String;
        
        address_t elementAddress = 0;
        auto elementCount = memoryReader.readArrayLength(address, entryType);
        if (elementType->typeIndex == snapshot->managedTypeIndex.system_Byte)
        {
            printf("%s└<%d>", __indent, elementCount);
            auto ptr = memoryReader.readMemory(address + __vm->arrayHeaderSize);
            printByteArray(ptr, elementCount);
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
//                    auto __type = &snapshot->typeDescriptions->items[typeIndex];
//                    if (deriveFromMType(*__type, elementType->typeIndex)) //
//                    {
//                        elementType = __type;
//                    }
                    elementType = &snapshot->typeDescriptions->items[typeIndex];
                }
                
                __antiCircular.insert(elementAddress);
            }
            
            closed ? memcpy(tabular, "└", 3) : memcpy(tabular, "├", 3);
            printf("%s[%d]:%s", __indent, i, elementType->name.c_str());
            if (__is_premitive)
            {
                printf(" = ");
                printPremitiveValue(elementAddress, elementType->typeIndex);
                printf("\n");
                continue;
            }
            
            if (elementAddress == 0)
            {
                printf(" = NULL\n");
                continue;
            }
            
            if (__is_string)
            {
                auto size = 0;
                printf(" 0x%08llx = '%s'\n", elementAddress, getUTFString(elementAddress, size, true).c_str());
                continue;
            }
            
            if (!elementType->isValueType) {printf(" 0x%08llx", elementAddress);}
            printf("\n");
            if (limit > 0 && __iter_depth + 1 >= limit) {continue;}
            if (closed)
            {
                char __nest_indent[__size + 1 + 2 + 1]; // indent + space + 2×space + \0
                memcpy(__nest_indent, indent, __size);
                memset(__nest_indent + __size, '\x20', 3);
                memset(__nest_indent + __size + 3, 0, 1);
                dumpMObjectHierarchy(elementAddress, elementType, __antiCircular, true, limit, __nest_indent, __iter_depth + 1);
            }
            else
            {
                char __nest_indent[__size + 3 + 2 + 1]; // indent + tabulator + 2×space + \0
                char *iter = __nest_indent + __size;
                memcpy(__nest_indent, indent, __size);
                memcpy(iter, "│", 3);
                memset(iter + 3, '\x20', 2);
                memset(iter + 5, 0, 1);
                dumpMObjectHierarchy(elementAddress, elementType, __antiCircular, true, limit, __nest_indent, __iter_depth + 1);
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
            iterType = &snapshot->typeDescriptions->items[iterType->baseOrElementTypeIndex];
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
        auto *fieldType = &snapshot->typeDescriptions->items[field.typeIndex];
        if (!fieldType->isValueType)
        {
            fieldAddress = memoryReader.readPointer(address + field.offset);
            if (field.typeIndex == snapshot->managedTypeIndex.system_String)
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
                fieldType = &snapshot->typeDescriptions->items[typeIndex];
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
        const char *fieldTypeName = fieldType->name.c_str();
        const char *fieldName = field.name.c_str();
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
                printPremitiveValue(fieldAddress, fieldType->typeIndex);
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
            if (limit > 0 && __iter_depth + 1 >= limit) {continue;}
            if (closed)
            {
                char __nest_indent[__size + 1 + 2 + 1]; // indent + space + 2×space + \0
                memcpy(__nest_indent, __indent, __size);
                memset(__nest_indent + __size, '\x20', 3);
                memset(__nest_indent + __size + 3, 0, 1);
                dumpMObjectHierarchy(fieldAddress, fieldType, __antiCircular, true, limit, __nest_indent, __iter_depth + 1);
            }
            else
            {
                char __nest_indent[__size + 3 + 2 + 1]; // indent + tabulator + 2×space + \0
                char *iter = __nest_indent + __size;
                memcpy(__nest_indent, __indent, __size);
                memcpy(iter, "│", 3);
                memset(iter + 3, '\x20', 2);
                memset(iter + 5, 0, 1);
                dumpMObjectHierarchy(fieldAddress, fieldType, __antiCircular, true, limit, __nest_indent, __iter_depth + 1);
            }
        }
    }
}

void MemorySnapshotCrawler::listMulticastDelegates()
{
    retrieveMulticastDelegate(0);
    
    vector<address_t> roots;
    map<address_t, bool> visit;
    for (auto i = __multicastForwardAddressMap.begin(); i != __multicastForwardAddressMap.end(); i++)
    {
        auto address = i->first;
        auto match = visit.find(address);
        if (match == visit.end())
        {
            visit.insert(pair<address_t, bool>(address, true));
            auto position = address;
            while (true)
            {
                auto iter = __multicastReverseAddressMap.find(position);
                if (iter == __multicastReverseAddressMap.end())
                {
                    roots.push_back(position);
                    break;
                }
                else
                {
                    position = match->second;
                }
            }
        }
    }
    
    map<int32_t, int32_t> counts;
    vector<int32_t> indice;
    
    for (auto i = roots.begin(); i != roots.end(); i++)
    {
        auto index = findMObjectAtAddress(*i);
        if (index == -1) {continue;}
        auto position = *i;
        auto refCount = 0;
        while (true)
        {
            refCount++;
            auto iter = __multicastForwardAddressMap.find(position);
            if (iter == __multicastForwardAddressMap.end()){break;}
            position = iter->second;
        }
        indice.push_back(index);
        counts.insert(pair<int32_t, int32_t>(index, refCount));
    }
    
    std::sort(indice.begin(), indice.end(), [&](int32_t a, int32_t b)
              {
                  auto ca = counts.at(a);
                  auto cb = counts.at(b);
                  if (ca != cb) {return ca > cb;}
                  return a < b;
              });
    for (auto i = indice.begin(); i != indice.end(); i++)
    {
        auto &mo = managedObjects[*i];
        auto &type = snapshot->typeDescriptions->items[mo.typeIndex];
        printf("\e[36m%3d 0x%08llx \e[32m%s \e[33m%s \e[37m*%d\n", counts.at(*i), mo.address, type.name.c_str(), type.assembly.c_str(), type.typeIndex);
    }
}

void MemorySnapshotCrawler::retrieveMulticastDelegate(address_t address)
{
    auto TypeIndexMulticastDelegate = snapshot->managedTypeIndex.system_MulticastDelegate;
    
    int32_t targetOffset = -1;
    int32_t prevOffset = -1;
    
    string prevString = "prev";
    string targetString = "m_target";
    
    map<int32_t, FieldDescription *> members;
    auto &TypeMulticastDelegate = snapshot->typeDescriptions->items[TypeIndexMulticastDelegate];
    if (TypeMulticastDelegate.typeIndex != TypeIndexMulticastDelegate) {return;}
    auto type = &TypeMulticastDelegate;
    while (true)
    {
        for (auto i = 0; i < type->fields->size; i++)
        {
            auto &field = type->fields->items[i];
            if (!field.isStatic)
            {
                members.insert(pair<int32_t, FieldDescription *>(field.offset, &field));
                if (field.name == targetString)
                {
                    targetOffset = field.offset;
                }
                else if (field.name == prevString)
                {
                    prevOffset = field.offset;
                }
            }
        }
        if (type->baseOrElementTypeIndex == -1){break;}
        type = &snapshot->typeDescriptions->items[type->baseOrElementTypeIndex];
    }
    
    if (prevOffset == -1 || targetOffset == -1) {return;}
    
    if (__multicastForwardAddressMap.size() == 0)
    {
        for (auto i = 0; i < managedObjects.size(); i++)
        {
            auto &mo = managedObjects[i];
            auto &mt = snapshot->typeDescriptions->items[mo.typeIndex];
            if (mt.baseOrElementTypeIndex == TypeIndexMulticastDelegate)
            {
                auto pointer = __memoryReader->readPointer(mo.address + prevOffset);
                if (pointer != 0)
                {
                    auto match = __multicastForwardAddressMap.find(mo.address);
                    assert(match == __multicastForwardAddressMap.end());
                    
                    __multicastForwardAddressMap.insert(pair<address_t, address_t>(mo.address, pointer));
                    __multicastReverseAddressMap.insert(pair<address_t, address_t>(pointer, mo.address));
                }
            }
        }
    }
    
    auto entityObjectIndex = findMObjectAtAddress(address);
    if (entityObjectIndex == -1) {return;}
    
    address_t position = address;
    while (true)
    {
        auto iter = __multicastReverseAddressMap.find(position);
        if (iter == __multicastReverseAddressMap.end()) {break;}
        position = iter->second;
    }
    
    vector<FieldDescription *> fields{members.at(prevOffset), members.at(targetOffset)};
    auto sourceIndex = findMObjectAtAddress(position);
    auto &source = managedObjects[sourceIndex];
    auto &sourceType = snapshot->typeDescriptions->items[source.typeIndex];
    auto selected = position == address;
    if (selected) {printf("\e[1m");}
    printf("%s 0x%08llx", sourceType.name.c_str(), position);
    if (selected) {printf("\e[0m\e[36m");}
    printf("\n");
    dumpMulticastDelegateHierarchy(position, address, fields, "");
}

void MemorySnapshotCrawler::dumpMulticastDelegateHierarchy(address_t address, address_t highlight, vector<FieldDescription *> &fields, const char *indent)
{
    auto __size = strlen(indent);
    char __indent[__size + 2*3 + 1]; // indent + 2×tabulator + \0
    memset(__indent, 0, sizeof(__indent));
    memcpy(__indent, indent, __size);
    char *tabular = __indent + __size;
    memcpy(tabular + 3, "─", 3);
    
    auto fieldCount = fields.size();
    for (auto i = 0; i < fieldCount; i++)
    {
        auto closed = i + 1 == fieldCount;
        closed ? memcpy(tabular, "└", 3) : memcpy(tabular, "├", 3);
        auto &field = *fields[i];
        auto *fieldType = &snapshot->typeDescriptions->items[field.typeIndex];
        auto fieldAddress = address + field.offset;
        auto isNull = false;
        if (fieldType->isValueType)
        {
            fieldAddress += __vm->objectHeaderSize;
        }
        else
        {
            fieldAddress = __memoryReader->readPointer(fieldAddress);
            if (fieldAddress == 0)
            {
                isNull = true;
            }
            else
            {
                auto typeIndex = findTypeOfAddress(fieldAddress);
                if (typeIndex >= 0)
                {
                    fieldType = &snapshot->typeDescriptions->items[typeIndex];
                }
            }
        }
        
        auto selected = fieldAddress == highlight;
        auto isMulticastDelegate = fieldType->baseOrElementTypeIndex == snapshot->managedTypeIndex.system_MulticastDelegate;
        printf("%s%s:", __indent, field.name.c_str());
        if (selected) {printf("\e[1m");}
        printf("%s", fieldType->name.c_str());
        if (isNull) {printf(" = NULL");}
        else if (!isMulticastDelegate) {printf(" = 0x%08llx", fieldAddress);}
        else {printf(" 0x%08llx", fieldAddress);}
        if (selected) {printf("\e[0m\e[36m");}
        printf("\n");
        
        if (isMulticastDelegate && !isNull)
        {
            dumpMulticastDelegateHierarchy(fieldAddress, highlight, fields, getNestIndent(__indent, __size, closed).c_str());
        }
    }
}

void MemorySnapshotCrawler::dumpUnbalancedEvents(MemoryState state)
{
    auto &delegateType = snapshot->typeDescriptions->items[snapshot->managedTypeIndex.system_Delegate];
    
    int32_t fieldOffset = 16;
    int32_t fieldTypeIndex = -1;
    for (auto i = 0; i < delegateType.fields->size; i++)
    {
        auto &field = delegateType.fields->items[i];
        if (field.name == "m_target")
        {
            fieldOffset = field.offset;
            fieldTypeIndex = field.typeIndex;
            break;
        }
    }
    
    using std::tuple;
    vector<tuple<address_t, int32_t, address_t, int32_t, int32_t>> records;
    map<int32_t, vector<int32_t>> listeners;
    vector<int32_t> indice;
    
    auto refMemory = 0;
    for (auto i = 0; i < managedObjects.size(); i++)
    {
        auto &mo = managedObjects[i];
        auto &type = snapshot->typeDescriptions->items[mo.typeIndex];
        
        if ((state == MS_none || state == mo.state) && deriveFromMType(type, delegateType.typeIndex))
        {
            auto ptr = __memoryReader->readPointer(mo.address + fieldOffset);
            if (ptr != 0)
            {
                auto typeIndex = findTypeOfAddress(ptr);
                if (typeIndex == -1)
                {
                    typeIndex = fieldTypeIndex;
                }
                
                auto __index = findMObjectAtAddress(ptr);
                assert(__index >= 0);
                
                auto &ref = managedObjects[__index];
                refMemory += ref.size;
                
                records.push_back(std::make_tuple(mo.address, mo.typeIndex, ptr, typeIndex, ref.size));
                
                auto match = listeners.find(__index);
                if (match == listeners.end())
                {
                    match = listeners.insert(pair<int32_t, vector<int32_t>>(__index, vector<int32_t>())).first;
                    indice.push_back(__index);
                }
                match->second.push_back(mo.managedObjectIndex);
            }
        }
    }
    
    using std::get;
    printf("[Unbalanced] count=%d ref_memory=%d\n", (int)records.size(), refMemory);
    for (auto iter = records.begin(); iter != records.end(); iter++)
    {
        auto &data = *iter;
        auto targetTypeIndex = get<3>(data);
        auto &targetType = snapshot->typeDescriptions->items[targetTypeIndex];
        
        auto &type = snapshot->typeDescriptions->items[get<1>(data)];
        
        printf("\e[36m0x%08llx type='%s'%d \e[32mtarget=[0x%08llx type='%s'%d size=%d]\n", get<0>(data), type.name.c_str(), type.typeIndex, get<2>(data), targetType.name.c_str(), targetType.typeIndex, get<4>(data));
    }
    
    std::sort(indice.begin(), indice.end(), [&](int32_t a, int32_t b)
              {
                  return listeners.at(a).size() > listeners.at(b).size();
              });
    
    auto digit = indice.size() == 0? 1 : (int)ceil(log10(indice.size()));
    
    char format[32];
    memset(format, 0, sizeof(format));
    sprintf(format, "\e[36m[%%0%dd/%d]", digit, (int32_t)indice.size());
    
    auto counter = 0;
    for (auto i = indice.begin(); i != indice.end(); i++)
    {
        auto iter = listeners.find(*i);
        auto &target = managedObjects[iter->first];
        auto &targetType = snapshot->typeDescriptions->items[target.typeIndex];
        auto &parents = iter->second;
        printf(format, ++counter);
        printf(" \e[32m0x%08llx type='%s'%d size=%d count=%d\n", target.address, targetType.name.c_str(), targetType.typeIndex, target.size, (int32_t)parents.size());
        for (auto p = parents.begin(); p != parents.end(); p++)
        {
            auto &parentObject = managedObjects[*p];
            auto &parentType = snapshot->typeDescriptions->items[parentObject.typeIndex];
            printf("  \e[33m+ 0x%08llx type='%s'%d\n", parentObject.address, parentType.name.c_str(), parentType.typeIndex);
        }
    }
}

void MemorySnapshotCrawler::dumpTransform(NativeTransform &transform)
{
    printf("position={x=%.3f, y=%.3f, z=%.3f} localPosition={x=%.3f, y=%.3f, z=%.3f} rotation={x=%.3f, y=%.3f, z=%.3f} localRotation={x=%.3f, y=%.3f, z=%.3f} scale={x=%.3f, y=%.3f, z=%.3f}",
           transform.position.x, transform.position.y, transform.position.z,
           transform.localPosition.x, transform.localPosition.y, transform.localPosition.z,
           transform.rotation.x, transform.rotation.y, transform.rotation.z,
           transform.localRotation.x, transform.localRotation.y, transform.localRotation.z,
           transform.scale.x, transform.scale.y, transform.scale.z);
}

void MemorySnapshotCrawler::inspectTransform(address_t address)
{
    auto index = findNObjectAtAddress(address);
    if (index >= 0)
    {
        auto &collection = snapshot->nativeAppendingCollection;
        auto &appending = collection.appendings[index];
        if (appending.transform != -1)
        {
            auto &transform = collection.transforms[appending.transform];
            dumpTransform(transform);
            printf("\n");
        }
        else if (appending.rectTransform != -1)
        {
            auto &transform = collection.rectTransforms[appending.rectTransform];
            dumpTransform(transform);
            printf(" rect={x=%.3f, y=%.3f, width=%.3f, height=%.3f} anchorMin={x=%.3f, y=%.3f} anchorMax={x=%.3f, y=%.3f} sizeDelta={x=%.3f, y=%.3f} pivot={x=%.3f, y=%.3f} anchoredPosition={x=%.3f, y=%.3f}",
                   transform.rect.x, transform.rect.y, transform.rect.width, transform.rect.height,
                   transform.anchorMin.x, transform.anchorMin.y,
                   transform.anchorMax.x, transform.anchorMax.y,
                   transform.sizeDelta.x, transform.sizeDelta.y,
                   transform.pivot.x, transform.pivot.y,
                   transform.anchoredPosition.x, transform.anchoredPosition.y);
            printf("\n");
        }
    }
}

void MemorySnapshotCrawler::inspectTexture2D(address_t address)
{
    auto index = findNObjectAtAddress(address);
    if (index >= 0)
    {
        auto &collection = snapshot->nativeAppendingCollection;
        auto &appending = collection.appendings[index];
        if (appending.texture != -1)
        {
            auto &tex = collection.textures[appending.texture];
            printf("pot=%s format=%d width=%d height=%d\n", tex.pot? "true" : "false", tex.format, tex.width, tex.height);
        }
    }
}

void MemorySnapshotCrawler::inspectSprite(address_t address)
{
    auto index = findNObjectAtAddress(address);
    if (index >= 0)
    {
        auto &collection = snapshot->nativeAppendingCollection;
        auto &appending = collection.appendings[index];
        if (appending.sprite != -1)
        {
            auto &sprite = collection.sprites[appending.sprite];
            printf("rect={x=%.3f, y=%.3f, width=%.3f, height=%.3f} pivot={x=%.3f, y=%.3f}\n",
                   sprite.x, sprite.y, sprite.width, sprite.height, sprite.pivot.x, sprite.pivot.y);
        }
    }
}

void MemorySnapshotCrawler::inspectComponent(address_t address)
{
    auto index = findNObjectAtAddress(address);
    if (index >= 0)
    {
        auto &map = snapshot->nativeAppendingCollection.componentAddressMap;
        auto match = map.find(address);
        if (match != map.end())
        {
            auto component = match->second;
            auto &go = snapshot->nativeObjects->items[component->gameObjectNativeArrayIndex];
            auto &no = snapshot->nativeObjects->items[component->nativeArrayIndex];
            auto &nt = snapshot->nativeTypes->items[no.nativeTypeArrayIndex];
            printf("0x%llx %s '%s'", component->address, nt.name.c_str(), no.name.c_str());
            if (component->behaviour)
            {
                printf(" enabled=%s isActiveAndEnabled=%s", component->enabled? "true":"false", component->isActiveAndEnabled?"true":"false");
            }
            printf(" GameObject[0x%llx '%s']\n", go.nativeObjectAddress, go.name.c_str());
        }
    }
}

void MemorySnapshotCrawler::inspectGameObject(address_t address)
{
    auto index = findNObjectAtAddress(address);
    auto &collection = snapshot->nativeAppendingCollection;
    if (index >= 0 && collection.appendings.size() > 0)
    {
        auto &appending = collection.appendings[index];
        if (appending.gameObject != -1)
        {
            auto &go = collection.gameObjects[appending.gameObject];
            auto &no = snapshot->nativeObjects->items[go.nativeArrayIndex];
            auto &nt = snapshot->nativeTypes->items[no.nativeTypeArrayIndex];
            printf("0x%llx '%s' %s isActive=%s isSelfActive=%s\n", appending.link.nativeAddress, no.name.c_str(), nt.name.c_str(), go.isActive? "true" : "false", go.isSelfActive? "true":"false");
            for (auto i = 0; i < go.components.size(); i++)
            {
                auto index = go.components[i];
                auto &component = collection.components[index];
                if (component.nativeArrayIndex == -1) {continue;}
                auto &cno = snapshot->nativeObjects->items[component.nativeArrayIndex];
                auto &cnt = snapshot->nativeTypes->items[cno.nativeTypeArrayIndex];
                printf("  - 0x%llx '%s' %s", component.address, cno.name.c_str(), cnt.name.c_str());
                if (component.behaviour)
                {
                    printf(" enabled=%s isActiveAndEnabled=%s", component.enabled? "true":"false", component.isActiveAndEnabled? "true":"false");
                }
                
                int32_t typeIndex = -1;
                auto &item = collection.appendings[component.nativeArrayIndex];
                auto mindex = findMObjectAtAddress(item.link.managedAddress);
                if (mindex == -1)
                {
                    typeIndex = findTypeOfAddress(item.link.managedAddress);
                }
                else
                {
                    typeIndex = managedObjects[mindex].typeIndex;
                }
                
                if (typeIndex == -1)
                {
                    typeIndex = findTypeAtTypeAddress(item.link.managedTypeAddress);
                }
                
                if (typeIndex >= 0)
                {
                    auto &cmt = snapshot->typeDescriptions->items[typeIndex];
                    printf(" M[0x%llx %s]", item.link.managedAddress, cmt.name.c_str());
                }
                printf("\n");
            }
        }
    }
}

void MemorySnapshotCrawler::crawlLinks()
{
    __sampler.begin("CrawlLinks");
    if (snapshot->nativeAppendingCollection.appendings.size() > 0)
    {
        auto &appendings = snapshot->nativeAppendingCollection.appendings;
        for (auto iter = appendings.begin(); iter != appendings.end(); iter++)
        {
            auto &joint = joints.add();
            joint.jointArrayIndex = joints.size() - 1;
            
            // set gcHandle info
            joint.gcHandleIndex = -1;
            joint.linkArrayIndex = iter->link.nativeArrayIndex;
            
            if (iter->link.managedTypeAddress != 0)
            {
                auto typeIndex = findTypeAtTypeAddress(iter->link.managedTypeAddress);
                if (typeIndex != -1)
                {
                    auto &type = snapshot->typeDescriptions->items[typeIndex];
                    crawlManagedEntryAddress(iter->link.managedAddress, &type, *__memoryReader, joint, true, 0);
                }
            }
        }
    }
    __sampler.end();
}

void MemorySnapshotCrawler::crawlGCHandles()
{
    __sampler.begin("CrawlGCHandles");
    auto &gcHandles = *snapshot->gcHandles;
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
    __sampler.begin("CrawlStatic");
    auto &typeDescriptions = *snapshot->typeDescriptions;
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
            auto *fieldType = &snapshot->typeDescriptions->items[field.typeIndex];
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
