//
//  main.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/2.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include <string>
#include <vector>
#include <locale>
#include <codecvt>
#include <stdlib.h>
#include <sys/stat.h>
#include <fstream>
#include "Crawler/crawler.h"
#include "Crawler/cache.h"
#include "Crawler/leak.h"
#include "Crawler/rserialize.h"
#include "Crawler/format.h"
#include "utils.h"

using std::cout;
using std::endl;
using std::vector;

void readCachedSnapshot(const char *uuid);

address_t castAddress(const char *v)
{
    if (v[0] == '0')
    {
        return strtoll(v, nullptr, 0);
    }
    else
    {
        return strtoll(v, nullptr, 10);
    }
}

PackedMemorySnapshot &deserialize(const char *filepath, PackedMemorySnapshot &snapshot)
{
    auto ptr = filepath;
    while (*ptr++ != 0) {}
    ptr -= 5;
    
    if (strbeg(ptr, ".pms"))
    {
        MemorySnapshotReader(filepath).read(snapshot);
    }
    else
    {
        // fallback format
        RawMemorySnapshotReader(filepath).read(snapshot);
    }

    return snapshot;
}

#include <unistd.h>
#include <memory>
using std::ofstream;
void processMemorySnapshot(const char * filepath)
{
    PackedMemorySnapshot snapshot;
    MemorySnapshotCrawler mainCrawler(&deserialize(filepath, snapshot));
    mainCrawler.crawl();
    
    auto filename = basename(filepath);
    
    char cmdpath[128];
    mkdir("__commands", 0777);
    memset(cmdpath, 0, sizeof(cmdpath));
    sprintf(cmdpath, "__commands/%s.mlog", filename.c_str());
    
    ofstream mlog;
    mlog.open(cmdpath, ofstream::app);
    
    char uuid[40];
    std::strcpy(uuid, mainCrawler.snapshot->uuid.c_str());
    MemoryState trackingMode = MS_none;
    
    std::istream *stream = &std::cin;
    while (true)
    {
        auto replaying = typeid(*stream) != typeid(std::cin);
        auto recordable = true;
        
        std::cout << "\e[93m/> ";
        
        READ_LINE:
        std::string input;
        while (!getline(*stream, input))
        {
            if (replaying)
            {
                ((ifstream *)stream)->close();
                stream = &std::cin;
                replaying = false;
            }
            stream->clear();
            replaying ? usleep(500000) : usleep(100000);
        }
        
        if (input.size() == 0 || input[0] == '#')
        {
            goto READ_LINE;
        }
        
        if (replaying)
        {
            auto iter = input.begin();
            while (iter != input.end())
            {
                cout << *iter++ << std::flush;
                usleep(50000);
            }
            cout << endl;
        }
        
        cout << "\e[0m" << "\e[36m";
        
        const char *command = input.c_str();
        if (strbeg(command, "read"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   PackedMemorySnapshot __snapshot;
                                   MemorySnapshotCrawler __crawler(&snapshot);
                                   if (options.size() == 1)
                                   {
                                       SnapshotCrawlerCache().read(uuid, &__crawler);
                                   }
                                   else
                                   {
                                       SnapshotCrawlerCache().read(options[1], &__crawler);
                                       mainCrawler.compare(__crawler);
                                   }
                               });
        }
        else if (strbeg(command, "save"))
        {
            SnapshotCrawlerCache().save(mainCrawler);
        }
        else if (strbeg(command, "load"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                if (options.size() == 1) {return;}
                PackedMemorySnapshot __snapshot;
                MemorySnapshotCrawler crawler(&deserialize(options[1], __snapshot));
                crawler.crawl();
                mainCrawler.compare(crawler);
            });
        }
        else if (strbeg(command, "uuid"))
        {
            printf("%s\n", mainCrawler.snapshot->uuid.c_str());
        }
        else if (strbeg(command, "link"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       printf("%lld\n", mainCrawler.findNObjectOfMObject(address));
                                   }
                               });
        }
        else if (strbeg(command, "ulink"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       printf("%lld\n", mainCrawler.findMObjectOfNObject(address));
                                   }
                               });
        }
        else if (strbeg(command, "KREF")) // complete managed object reference chains except circular ones
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() > 1)
                                   {
                                       mainCrawler.dumpMRefChain(castAddress(options[1]), false, -1,
                                                                 options.size() > 2 ? atoi(options[2]) : -1);
                                   }
                               });
        }
        else if (strbeg(command, "UKREF")) // complete native object reference chains except circular ones
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() > 1)
                                   {
                                       mainCrawler.dumpNRefChain(castAddress(options[1]), false, -1,
                                                                 options.size() > 2 ? atoi(options[2]) : -1);
                                   }
                               });
        }
        else if (strbeg(command, "REF")) // complete managed object reference chains
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() > 1)
                                   {
                                       mainCrawler.dumpMRefChain(castAddress(options[1]), true, -1,
                                                                 options.size() > 2 ? atoi(options[2]) : -1);
                                   }
                               });
        }
        else if (strbeg(command, "UREF")) // complete native object reference chains
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() > 1)
                                   {
                                       mainCrawler.dumpNRefChain(castAddress(options[1]), true, -1,
                                                                 options.size() > 2 ? atoi(options[2]) : -1);
                                   }
                               });
        }
        else if (strbeg(command, "kref")) // partial managed object reference chains except circular ones
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() > 1)
                                   {
                                       mainCrawler.dumpMRefChain(castAddress(options[1]), false,
                                                                 options.size() > 2 ? atoi(options[2]) : 2,
                                                                 options.size() > 3 ? atoi(options[3]) : -1);
                                   }
                               });
        }
        else if (strbeg(command, "ukref")) // partial native object reference chains except circular ones
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() > 1)
                                   {
                                       mainCrawler.dumpNRefChain(castAddress(options[1]), false,
                                                                 options.size() > 2 ? atoi(options[2]) : 2,
                                                                 options.size() > 3 ? atoi(options[3]) : -1);
                                   }
                               });
        }
        else if (strbeg(command, "ref")) // partial managed object reference chains
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() > 1)
                                   {
                                       mainCrawler.dumpMRefChain(castAddress(options[1]), true,
                                                                 options.size() > 2 ? atoi(options[2]) : 2,
                                                                 options.size() > 3 ? atoi(options[3]) : -1);
                                   }
                               });
        }
        else if (strbeg(command, "vref")) // partial managed object reference chains
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       mainCrawler.dumpVRefChain(castAddress(options[i]));
                                   }
                               });
        }
        else if (strbeg(command, "uref")) // partial native object reference chains
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() > 1)
                                   {
                                       mainCrawler.dumpNRefChain(castAddress(options[1]), true,
                                                                 options.size() > 2 ? atoi(options[2]) : 2,
                                                                 options.size() > 3 ? atoi(options[3]) : -1);
                                   }
                               });
        }
        else if (strbeg(command, "comp")) // partial native object reference chains
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       mainCrawler.inspectComponent(castAddress(options[i]));
                                   }
                               });
        }
        else if (strbeg(command, "tfm")) // partial native object reference chains
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       mainCrawler.inspectTransform(castAddress(options[i]));
                                   }
                               });
        }
        else if (strbeg(command, "tex")) // partial native object reference chains
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       mainCrawler.inspectTexture2D(castAddress(options[i]));
                                   }
                               });
        }
        else if (strbeg(command, "sprite")) // partial native object reference chains
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       mainCrawler.inspectSprite(castAddress(options[i]));
                                   }
                               });
        }
        else if (strbeg(command, "go")) // partial native object reference chains
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       mainCrawler.inspectGameObject(castAddress(options[i]));
                                   }
                               });
        }
        else if (strbeg(command, "replay"))
        {
            recordable = false;
            std::vector<string> commands;
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   auto ifs = new ifstream;
                                   if (options.size() == 1)
                                   {
                                       ifs->open(cmdpath);
                                   }
                                   else
                                   {
                                       ifs->open(options[1]);
                                   }
                                   stream = ifs;
                               });
        }
        else if (strbeg(command, "leak"))
        {
            readCommandOptions(command, [](std::vector<const char *> &options)
                               {
                                   if (options.size() == 1){return;}
                                   const char *subcommand = options[1];
                                   inspectCondition<PackedNativeUnityEngineObject>(subcommand);
                               });
        }
        else if (strbeg(command, "track"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() == 1)
                                   {
                                       trackingMode = MS_none;
                                   }
                                   else
                                   {
                                       auto subcommand = options[1];
                                       if (0 == strcmp(subcommand, "alloc"))
                                       {
                                           trackingMode = MS_allocated;
                                       }
                                       else if (0 == strcmp(subcommand, "leak"))
                                       {
                                           trackingMode = MS_persistent;
                                       }
                                       else if (0 == strcmp(subcommand, "?")) {}
                                       else
                                       {
                                           trackingMode = MS_none;
                                       }
                                   }
                                   switch (trackingMode)
                                   {
                                       case MS_none:
                                           printf("\e[37m\e[7mLEAVE TRACKING MODE\e[0m\n");
                                           break;
                                       case MS_allocated:
                                           printf("\e[92m\e[7mENTER TRACKING ALLOC MODE\e[0m\n");
                                           break;
                                       case MS_persistent:
                                           printf("\e[92m\e[7mENTER TRACKING LEAK MODE\e[0m\n");
                                           break;
                                   }
                               });
        }
        else if (strbeg(command, "stat"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   if (options.size() == 1)
                                   {
                                       mainCrawler.trackMStatistics(trackingMode, 5);
                                   }
                                   else
                                   {
                                       mainCrawler.trackMStatistics(trackingMode, atoi(options[1]));
                                   }
                               });
        }
        else if (strbeg(command, "ustat"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   if (options.size() == 1)
                                   {
                                       mainCrawler.trackNStatistics(trackingMode, 5);
                                   }
                                   else
                                   {
                                       mainCrawler.trackNStatistics(trackingMode, atoi(options[1]));
                                   }
                               });
        }
        else if (strbeg(command, "list"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   if (options.size() >= 2)
                                   {
                                       mainCrawler.trackMTypeObjects(trackingMode, atoi(options[1]),
                                                                     options.size() >= 3? atoi(options[2]) : 0,
                                                                     options.size() >= 4? atoi(options[3]) : 1);
                                   }
                               });
        }
        else if (strbeg(command, "ulist"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   if (options.size() >= 2)
                                   {
                                       mainCrawler.trackNTypeObjects(trackingMode, atoi(options[1]), options.size() >= 3? atoi(options[2]) : 0);
                                   }
                               });
        }
        else if (strbeg(command, "show"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   if (options.size() >= 2)
                                   {
                                       int32_t depth = 0;
                                       if (options.size() >= 3) { depth = atoi(options[2]); }
                                       mainCrawler.inspectMObject(castAddress(options[1]), depth);
                                   }
                               });
        }
        else if (strbeg(command, "ushow"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   if (options.size() >= 2)
                                   {
                                       int32_t depth = 0;
                                       if (options.size() >= 3) { depth = atoi(options[2]); }
                                       mainCrawler.inspectNObject(castAddress(options[1]), depth);
                                   }
                               });
        }
        else if (strbeg(command, "vshow"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       mainCrawler.inspectVObject(castAddress(options[i]));
                                   }
                               });
        }
        else if (strbeg(command, "find"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       mainCrawler.findMObject(castAddress(options[i]));
                                   }
                               });
        }
        else if (strbeg(command, "ufind"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       mainCrawler.findNObject(castAddress(options[i]));
                                   }
                               });
        }
        else if (strbeg(command, "type"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       mainCrawler.inspectMType(atoi(options[i]));
                                   }
                               });
        }
        else if (strbeg(command, "utype"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       mainCrawler.inspectNType(atoi(options[i]));
                                   }
                               });
        }
        else if (strbeg(command, "bar"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   if (options.size() == 1)
                                   {
                                       mainCrawler.barMMemory(trackingMode, 20);
                                   }
                                   else
                                   {
                                       for (auto i = 1; i < options.size(); i++)
                                       {
                                           mainCrawler.barMMemory(trackingMode, atoi(options[i]));
                                       }
                                   }
                               });
        }
        else if (strbeg(command, "ubar"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   if (options.size() == 1)
                                   {
                                       mainCrawler.barNMemory(trackingMode, 20);
                                   }
                                   else
                                   {
                                       for (auto i = 1; i < options.size(); i++)
                                       {
                                           mainCrawler.barNMemory(trackingMode, atoi(options[i]));
                                       }
                                   }
                               });
        }
        else if (strbeg(command, "top"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   mainCrawler.topMObjects(options.size() == 1 ? 50 : atoi(options[1]),
                                                           options.size() > 2? castAddress(options[2]) : 0,
                                                           options.size() > 3? strcmp(options[3], "true") == 0 : false);
                               });
        }
        else if (strbeg(command, "utop"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   mainCrawler.topNObjects(options.size() == 1 ? 50 : atoi(options[1]));
                               });
        }
        else if (strbeg(command, "frag"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   mainCrawler.statFragments();
                               });
        }
        else if (strbeg(command, "draw"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   mainCrawler.drawUsedHeapGraph(filename.c_str(), options.size() > 1 && strcmp(options[1], "true") == 0);
                               });
        }
        else if (strbeg(command, "heap"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   if (options.size() == 1)
                                   {
                                       mainCrawler.statHeap();
                                   }
                                   else
                                   {
                                       mainCrawler.statHeap(atoi(options[1]));
                                   }
                               });
        }
        else if (strbeg(command, "iheap"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                if (options.size() == 1)
                {
                    mainCrawler.inspectHeap();
                }
                else
                {
                    if (strbeg(options[1], "save"))
                    {
                        mainCrawler.inspectHeap(filename.c_str());
                    }
                    else if (strbeg(options[1], "draw"))
                    {
                        mainCrawler.drawHeapGraph(filename.c_str(),
                                                  options.size() > 2 && strcmp(options[2], "true") == 0);
                    }
                        
                }
            });
        }
        else if (strbeg(command, "base"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   if (options.size() == 1)
                                   {
                                       mainCrawler.statSubclasses();
                                   }
                                   else
                                   {
                                       mainCrawler.dumpSubclassesOf(atoi(options[1]));
                                   }
                               });
        }
        else if (strbeg(command, "event"))
        {
            mainCrawler.dumpUnbalancedEvents(trackingMode);
        }
        else if (strbeg(command, "class"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   if (options.size() == 1)
                                   {
                                       // list all class types
                                       mainCrawler.dumpAllClasses();
                                   }
                                   else
                                   {
                                       // find class by name
                                       for (auto i = 1; i < options.size(); i++)
                                       {
                                           mainCrawler.findClass(options[i]);
                                       }
                                   }
                               });
        }
        else if (strbeg(command, "uname"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   if (options.size() > 1)
                                   {
                                       mainCrawler.findNObject(options[1], options.size() > 2? strcmp(options[2], "true") == 0 : false);
                                   }
                               });
        }
        else if (strbeg(command, "static"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   if (options.size() == 1)
                                   {
                                       // list static bytes size descending
                                       mainCrawler.listAllStatics();
                                   }
                                   else
                                   {
                                       // show static field values
                                       mainCrawler.dumpStatic(atoi(options[1]),
                                                              options.size() > 2 ? (atoi(options[2]) != 0) : false);
                                   }
                               });
        }
        else if (strbeg(command, "delg"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   if (options.size() == 1)
                                   {
                                       mainCrawler.listMulticastDelegates();
                                   }
                                   else
                                   {
                                       for (auto i = 1; i < options.size(); i++)
                                       {
                                           mainCrawler.retrieveMulticastDelegate(castAddress(options[i]));
                                       }
                                   }
                               });
        }
        else if (strbeg(command, "str"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convertor;
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       int32_t size = 0;
                                       auto data = mainCrawler.getString(address, size);
                                       if (data == nullptr) {continue;}
                                       printf("0x%08llx %d \e[32m'%s'\n", address, size, convertor.to_bytes(data).c_str());
                                   }
                               });
        }
        else if (strbeg(command, "dup"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   if (options.size() == 1)
                                   {
                                       mainCrawler.dumpRepeatedObjects(mainCrawler.snapshot->managedTypeIndex.system_String);
                                   }
                                   else
                                   {
                                       mainCrawler.dumpRepeatedObjects(atoi(options[1]), options.size() >= 3? atoi(options[2]) : 2);
                                   }
                               });
        }
        else if (strbeg(command, "handle"))
        {
            mainCrawler.dumpGCHandles();
        }
        else if (strbeg(command, "export"))
        {
            char exportpath[256];
            mkdir("__export", 0777);
            sprintf(exportpath, "__export/%s.heap", filename.c_str());
            HeapExplorerFormat().encode(&snapshot, exportpath);
        }
        else if (strbeg(command, "quit"))
        {
            recordable = false;
            printf("\e[0m");
            mlog.close();
            return;
        }
        else if (strbeg(command, "help"))
        {
            recordable = false;
            const int __indent = 6;
            help("read", "[UUID]*", "读取以sqlite3保存的内存快照缓存", __indent);
            help("load", "[PMS_FILE_PATH]*", "加载内存快照文件", __indent);
            help("track", "[alloc|leak]", "追踪内存增长以及泄露问题", __indent);
            help("str", "[ADDRESS]*", "解析地址对应的字符串内容", __indent);
            help("ref", "[ADDRESS]*", "列举保持托管对象内存活跃的引用关系", __indent);
            help("vref", "[ADDRESS]*", "列举托管内存里面数值类型对象的的引用路径", __indent);
            help("uref", "[ADDRESS]*", "列举保持引擎对象内存活跃的引用关系", __indent);
            help("REF", "[ADDRESS]*", "列举保持托管对象内存活跃的全量引用关系", __indent);
            help("UREF", "[ADDRESS]*", "列举保持引擎对象内存活跃的全量引用关系", __indent);
            help("kref", "[ADDRESS]*", "列举保持托管对象内存活跃的引用关系并剔除干扰项", __indent);
            help("ukref", "[ADDRESS]*", "列举保持引擎对象内存活跃的引用关系并剔除干扰项", __indent);
            help("KREF", "[ADDRESS]*", "列举保持托管对象内存活跃的全量引用关系并剔除干扰项", __indent);
            help("UKREF", "[ADDRESS]*", "列举保持引擎对象内存活跃的全量引用关系并剔除干扰项", __indent);
            help("link", "[ADDRESS]*", "查看与托管对象链接的引擎对象", __indent);
            help("ulink", "[ADDRESS]*", "查看与引擎对象链接的托管对象", __indent);
            help("show", "[ADDRESS]* [DEPTH]", "查看托管对象内存排布以及变量值", __indent);
            help("vshow", "[ADDRESS]*", "查看托管内存对象的内存数据", __indent);
            help("ushow", "[ADDRESS]* [DEPTH]", "查看引擎对象内部的引用关系", __indent);
            help("find", "[ADDRESS]*", "查找托管对象", __indent);
            help("ufind", "[ADDRESS]*", "查找引擎对象", __indent);
            help("type", "[TYPE_INDEX]*", "查看托管类型信息", __indent);
            help("utype", "[TYPE_INDEX]*", "查看引擎类型信息", __indent);
            help("dup", "[TYPE_INDEX]", "按指定类型统计相同对象的信息", __indent);
            help("stat", "[RANK]", "按类型输出托管对象内存占用前RANK名的简报[支持内存追踪过滤]", __indent);
            help("ustat", "[RANK]", "按类型输出引擎对象内存占用前RANK名的简报[支持内存追踪过滤]", __indent);
            help("bar", "[RANK]", "输出托管类型内存占用前RANK名图形简报[支持内存追踪过滤]", __indent);
            help("ubar", "[RANK]", "输出引擎类型内存占用前RANK名图形简报[支持内存追踪过滤]", __indent);
            help("top", "[RANK] [ADDRESS] [KEEP_ADDRESS_ORDER]", "按大小降序/原序输出最大的内存对象列表", __indent);
            help("utop", "[RANK]", "按大小降序输出输出Native对象列表", __indent);
            help("list", NULL, "列举托管类型所有活跃对象内存占用简报[支持内存追踪过滤]", __indent);
            help("ulist", NULL, "列举引擎类型所有活跃对象内存占用简报[支持内存追踪过滤]", __indent);
            help("event", NULL, "搜索所有未清理的delegate对象");
            help("delg", "[ADDRESS]*", "查看MulticastDelegate链表");
            help("heap", "[RANK]", "输出动态内存简报", __indent);
            help("iheap", "[save|draw]", "输出动态内存简报", __indent);
            help("draw", "[SORT_SECTION_BY_SIZE]", "生成SVG文件可视化内存使用情况，用来定位内存碎片问题", __indent);
            help("frag", NULL, "输出内存碎片信息", __indent);
            help("go", "[ADDRESS]*", "输出GameObject对象的entity-components信息", __indent);
            help("comp", "[ADDRESS]*", "输出Native组件信息", __indent);
            help("tfm", "[ADDRESS]*", "输出Transform/RectTransform内存值信息", __indent);
            help("tex", "[ADDRESS]*", "输出Texture2D资源信息", __indent);
            help("sprite", "[ADDRESS]*", "输出Sprite资源信息", __indent);
            help("base", "[TYPE_INDEX]", "查看当前类型的子类型", __indent);
            help("save", NULL, "把当前内存快照分析结果以sqlite3格式保存到本机", __indent);
            help("uuid", NULL, "查看内存快照UUID", __indent);
            help("handle", NULL, "查看GCHandle对象", __indent);
            help("static", "[TYPE_INDEX]", "查看类静态对象数据", __indent);
            help("class", "[CLASS_NAME]", "查看类信息", __indent);
            help("uname", "[UNITY_ASSET_NAME]", "列举名字以指定字符开头的所有Native对象", __indent);
            help("help", NULL, "帮助", __indent);
            help("quit", NULL, "退出", __indent);
            cout << std::flush;
        }
        else if (strbeg(command, "color"))
        {
            for (auto i = 1; i <= 256; i++)
            {
                printf("\e[%dm\\e[%dm後沐白@moobyte\\e[0m\e[0m\n", i, i);
            }
        }
        else
        {
            printf("not supported command [%s]\n", command);
            recordable = false;
        }
        
        if (!replaying && recordable)
        {
            mlog.clear();
            mlog << input.c_str() << endl;
        }
    }
}

int main(int argc, const char * argv[])
{
#if DEBUG
    cout << "argc=" << argc << endl;
    for (auto i = 0; i < argc; i++)
    {
        cout << "argv[" << i << "]=" << argv[i] << endl;
    }
    
#endif
    
    if (argc > 1)
    {
        processMemorySnapshot(argv[1]);
    }
    
    return 0;
}
