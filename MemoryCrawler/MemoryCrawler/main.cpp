//
//  main.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/2.
//  Copyright ? 2019 larryhou. All rights reserved.
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
#include "Crawler/types.h"

using std::cout;
using std::endl;
using std::vector;
using std::ofstream;

void readCachedSnapshot(const char *uuid);
void printHit();
void printOutputHit();

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


#if _MSC_VER

void printHit()
{
	std::cout << "/> ";
}

void printOutputHit()
{
	std::cout << "";
}

#else

void printHit()
{
	std::cout << "\e[93m/> ";
}

void printOutputHit()
{
	cout << "\e[0m" << "\e[36m";
}

#endif



void processMemorySnapshot(const char * filepath)
{
    PackedMemorySnapshot snapshot;
    MemorySnapshotCrawler mainCrawler(&deserialize(filepath, snapshot));
    mainCrawler.crawl();
    
    auto filename = basename(filepath);
    
    char cmdpath[128];
	crawler_mkdir("__commands");
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
        
		printHit();
        
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
			if (replaying)
			{
				crawler_sleep(500000);
			}
			else
			{
				crawler_sleep(100000);
			}
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
				crawler_sleep(50000);
            }
            cout << endl;
        }
        
		printOutputHit();
        
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
        else if (strbeg(command, "size"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       std::set<address_t> antiCircular;
                                       mainCrawler.getReferencedMemoryOf(address, nullptr, antiCircular, true);
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
                                       mainCrawler.findClass(options[1], options.size() > 2? strcmp(options[2], "true") != 0 : true);
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
                                   //std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convertor;
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       int32_t size = 0;
                                       auto data = mainCrawler.getString(address, size);
                                       if (data == nullptr) {continue;}
                                       printf("0x%08llx %d \e[32m'%s'\n", address, size, utf16_to_utf8(data).c_str());
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
			crawler_mkdir("__export");
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
            help("read", "[UUID]*", "��ȡ��sqlite3������ڴ���ջ���", __indent);
            help("load", "[PMS_FILE_PATH]*", "�����ڴ�����ļ�", __indent);
            help("track", "[alloc|leak]", "׷���ڴ������Լ�й¶����", __indent);
            help("str", "[ADDRESS]*", "������ַ��Ӧ���ַ�������", __indent);
            help("ref", "[ADDRESS]*", "�оٱ����йܶ����ڴ��Ծ�����ù�ϵ", __indent);
            help("vref", "[ADDRESS]*", "�о��й��ڴ�������ֵ���Ͷ���ĵ�����·��", __indent);
            help("uref", "[ADDRESS]*", "�оٱ�����������ڴ��Ծ�����ù�ϵ", __indent);
            help("REF", "[ADDRESS]*", "�оٱ����йܶ����ڴ��Ծ��ȫ�����ù�ϵ", __indent);
            help("UREF", "[ADDRESS]*", "�оٱ�����������ڴ��Ծ��ȫ�����ù�ϵ", __indent);
            help("kref", "[ADDRESS]*", "�оٱ����йܶ����ڴ��Ծ�����ù�ϵ���޳�������", __indent);
            help("ukref", "[ADDRESS]*", "�оٱ�����������ڴ��Ծ�����ù�ϵ���޳�������", __indent);
            help("KREF", "[ADDRESS]*", "�оٱ����йܶ����ڴ��Ծ��ȫ�����ù�ϵ���޳�������", __indent);
            help("UKREF", "[ADDRESS]*", "�оٱ�����������ڴ��Ծ��ȫ�����ù�ϵ���޳�������", __indent);
            help("link", "[ADDRESS]*", "�鿴���йܶ������ӵ��������", __indent);
            help("ulink", "[ADDRESS]*", "�鿴������������ӵ��йܶ���", __indent);
            help("show", "[ADDRESS]* [DEPTH]", "�鿴�йܶ����ڴ��Ų��Լ�����ֵ", __indent);
            help("vshow", "[ADDRESS]*", "�鿴�й��ڴ������ڴ�����", __indent);
            help("ushow", "[ADDRESS]* [DEPTH]", "�鿴��������ڲ������ù�ϵ", __indent);
            help("find", "[ADDRESS]*", "�����йܶ���", __indent);
            help("ufind", "[ADDRESS]*", "�����������", __indent);
            help("size", "[ADDRESS]*", "�����йܶ������õ��ڴ��С", __indent);
            help("type", "[TYPE_INDEX]*", "�鿴�й�������Ϣ", __indent);
            help("utype", "[TYPE_INDEX]*", "�鿴����������Ϣ", __indent);
            help("dup", "[TYPE_INDEX]", "��ָ������ͳ����ͬ�������Ϣ", __indent);
            help("stat", "[RANK]", "����������йܶ����ڴ�ռ��ǰRANK���ļ�[֧���ڴ�׷�ٹ���]", __indent);
            help("ustat", "[RANK]", "�����������������ڴ�ռ��ǰRANK���ļ�[֧���ڴ�׷�ٹ���]", __indent);
            help("bar", "[RANK]", "����й������ڴ�ռ��ǰRANK��ͼ�μ�[֧���ڴ�׷�ٹ���]", __indent);
            help("ubar", "[RANK]", "������������ڴ�ռ��ǰRANK��ͼ�μ�[֧���ڴ�׷�ٹ���]", __indent);
            help("top", "[RANK] [ADDRESS] [KEEP_ADDRESS_ORDER]", "����С����/ԭ����������ڴ�����б�", __indent);
            help("utop", "[RANK]", "����С����������Native�����б�", __indent);
            help("list", NULL, "�о��й��������л�Ծ�����ڴ�ռ�ü�[֧���ڴ�׷�ٹ���]", __indent);
            help("ulist", NULL, "�о������������л�Ծ�����ڴ�ռ�ü�[֧���ڴ�׷�ٹ���]", __indent);
            help("event", NULL, "��������δ�����delegate����");
            help("delg", "[ADDRESS]*", "�鿴MulticastDelegate����");
            help("heap", "[RANK]", "�����̬�ڴ��", __indent);
            help("iheap", "[save|draw]", "�����̬�ڴ��", __indent);
            help("draw", "[SORT_SECTION_BY_SIZE]", "����SVG�ļ����ӻ��ڴ�ʹ�������������λ�ڴ���Ƭ����", __indent);
            help("frag", NULL, "����ڴ���Ƭ��Ϣ", __indent);
            help("go", "[ADDRESS]*", "���GameObject�����entity-components��Ϣ", __indent);
            help("comp", "[ADDRESS]*", "���Native�����Ϣ", __indent);
            help("tfm", "[ADDRESS]*", "���Transform/RectTransform�ڴ�ֵ��Ϣ", __indent);
            help("tex", "[ADDRESS]*", "���Texture2D��Դ��Ϣ", __indent);
            help("sprite", "[ADDRESS]*", "���Sprite��Դ��Ϣ", __indent);
            help("base", "[TYPE_INDEX]", "�鿴��ǰ���͵�������", __indent);
            help("save", NULL, "�ѵ�ǰ�ڴ���շ��������sqlite3��ʽ���浽����", __indent);
            help("uuid", NULL, "�鿴�ڴ����UUID", __indent);
            help("handle", NULL, "�鿴GCHandle����", __indent);
            help("static", "[TYPE_INDEX]", "�鿴�ྲ̬��������", __indent);
            help("class", "[CLASS_NAME]", "�鿴����Ϣ", __indent);
            help("uname", "[UNITY_ASSET_NAME]", "�о�������ָ���ַ���ͷ������Native����", __indent);
            help("help", NULL, "����", __indent);
            help("quit", NULL, "�˳�", __indent);
            cout << std::flush;
        }
        else if (strbeg(command, "color"))
        {
            for (auto i = 1; i <= 256; i++)
            {
                printf("\e[%dm\\e[%dm�����@moobyte\\e[0m\e[0m\n", i, i);
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
