﻿//
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
#if defined _MSC_VER
#include <direct.h>
#else 
#include <sys/stat.h>
#endif
#include <fstream>
#include "Crawler/crawler.h"
#include "Crawler/cache.h"
#include "Crawler/leak.h"
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

#include <chrono>
#include <thread>
#include <memory>
using std::ofstream;
void processRecord(const char * filepath)
{
    MemorySnapshotCrawler mainCrawler(filepath);
    mainCrawler.crawl();
    
    auto filename = basename(filepath);
    
    char cmdpath[128];

#if defined _MSC_VER
    _mkdir("__commands");
#else
    mkdir("__commands", 0777);
#endif

    memset(cmdpath, 0, sizeof(cmdpath));
    sprintf(cmdpath, "__commands/%s.mlog", filename);
    delete [] filename;
    
    ofstream mlog;
    mlog.open(cmdpath, ofstream::app);
    
    char uuid[40];
    std::strcpy(uuid, mainCrawler.snapshot.uuid.c_str());
    MemoryState trackingMode = MS_none;
    
    std::istream *stream = &std::cin;
    while (true)
    {
        auto replaying = typeid(*stream) != typeid(std::cin);
        auto recordable = true;
        
        std::cout << "\e[93m/> ";
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
            replaying ? std::this_thread::sleep_for(std::chrono::microseconds(500000)) 
            : std::this_thread::sleep_for(std::chrono::microseconds(100000));
        }
        
        if (replaying)
        {
            auto iter = input.begin();
            while (iter != input.end())
            {
                cout << *iter++ << std::flush;
                std::this_thread::sleep_for(std::chrono::microseconds(50000));
            }
            cout << endl;
        }
        
        cout << "\e[0m" << "\e[36m";
        
        const char *command = input.c_str();
        if (strbeg(command, "read"))
        {
            MemorySnapshotCrawler *crawler = nullptr;
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() == 1)
                                   {
                                       crawler = SnapshotCrawlerCache().read(uuid);
                                   }
                                   else
                                   {
                                       crawler = SnapshotCrawlerCache().read(options[1]);
                                       mainCrawler.compare(*crawler);
                                   }
                               });
            delete crawler;
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
                                   MemorySnapshotCrawler crawler(options[1]);
                                   crawler.crawl();
                                   mainCrawler.compare(crawler);
                               });
        }
        else if (strbeg(command, "uuid"))
        {
            printf("%s\n", mainCrawler.snapshot.uuid.c_str());
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
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       mainCrawler.dumpMRefChain(address, false, -1);
                                   }
                               });
        }
        else if (strbeg(command, "UKREF")) // complete native object reference chains except circular ones
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       mainCrawler.dumpNRefChain(address, false, -1);
                                   }
                               });
        }
        else if (strbeg(command, "REF")) // complete managed object reference chains
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       mainCrawler.dumpMRefChain(address, true, -1);
                                   }
                               });
        }
        else if (strbeg(command, "UREF")) // complete native object reference chains
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       mainCrawler.dumpNRefChain(address, true, -1);
                                   }
                               });
        }
        else if (strbeg(command, "kref")) // partial managed object reference chains except circular ones
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       mainCrawler.dumpMRefChain(address, false);
                                   }
                               });
        }
        else if (strbeg(command, "ukref")) // partial native object reference chains except circular ones
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       mainCrawler.dumpNRefChain(address, false);
                                   }
                               });
        }
        else if (strbeg(command, "ref")) // partial managed object reference chains
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       mainCrawler.dumpMRefChain(address, true);
                                   }
                               });
        }
        else if (strbeg(command, "uref")) // partial native object reference chains
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       mainCrawler.dumpNRefChain(address, true);
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
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       mainCrawler.trackMTypeObjects(trackingMode, atoi(options[i]));
                                   }
                               });
        }
        else if (strbeg(command, "ulist"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       mainCrawler.trackNTypeObjects(trackingMode, atoi(options[i]));
                                   }
                               });
        }
        else if (strbeg(command, "show"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       mainCrawler.inspectMObject(castAddress(options[i]));
                                   }
                               });
        }
        else if (strbeg(command, "ushow"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       mainCrawler.inspectNObject(castAddress(options[i]));
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
        else if (strbeg(command, "str"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
#if defined _MSC_VER
                                   std::wstring_convert<std::codecvt_utf8<int16_t>, int16_t> convertor;
#else
                                   std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convertor;
#endif
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       int32_t size = 0;
                                       auto data = mainCrawler.getString(address, size);
                                       if (data == nullptr) {continue;}
#if defined _MSC_VER
                                       printf("0x%08llx %d \e[32m'%s'\n", address, size, convertor.to_bytes(reinterpret_cast<const int16_t *>(data)).c_str());
#else
                                       printf("0x%08llx %d \e[32m'%s'\n", address, size, convertor.to_bytes(data).c_str());
#endif
                                   }
                               });
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
            const int __indent = 5;
            help("read", "[UUID]*", "读取以sqlite3保存的内存快照缓存", __indent);
            help("load", "[PMS_FILE_PATH]*", "加载内存快照文件", __indent);
            help("track", "[alloc|leak]", "追踪内存增长以及泄露问题", __indent);
            help("str", "[ADDRESS]*", "解析地址对应的字符串内容", __indent);
            help("ref", "[ADDRESS]*", "列举保持IL2CPP对象内存活跃的引用关系", __indent);
            help("uref", "[ADDRESS]*", "列举保持引擎对象内存活跃的引用关系", __indent);
            help("REF", "[ADDRESS]*", "列举保持IL2CPP对象内存活跃的全量引用关系", __indent);
            help("UREF", "[ADDRESS]*", "列举保持引擎对象内存活跃的全量引用关系", __indent);
            help("kref", "[ADDRESS]*", "列举保持IL2CPP对象内存活跃的引用关系并剔除干扰项", __indent);
            help("ukref", "[ADDRESS]*", "列举保持引擎对象内存活跃的引用关系并剔除干扰项", __indent);
            help("KREF", "[ADDRESS]*", "列举保持IL2CPP对象内存活跃的全量引用关系并剔除干扰项", __indent);
            help("UKREF", "[ADDRESS]*", "列举保持引擎对象内存活跃的全量引用关系并剔除干扰项", __indent);
            help("link", "[ADDRESS]*", "查看与IL2CPP对象链接的引擎对象", __indent);
            help("ulink", "[ADDRESS]*", "查看与引擎对象链接的IL2CPP对象", __indent);
            help("show", "[ADDRESS]*", "查看IL2CPP对象内存排布以及变量值", __indent);
            help("ushow", "[ADDRESS]*", "查看引擎对象内部的引用关系", __indent);
            help("find", "[ADDRESS]*", "查找IL2CPP对象", __indent);
            help("ufind", "[ADDRESS]*", "查找引擎对象", __indent);
            help("type", "[TYPE_INDEX]*", "查看IL2CPP类型信息", __indent);
            help("utype", "[TYPE_INDEX]*", "查看引擎类型信息", __indent);
            help("stat", "[RANK]", "按类型输出IL2CPP对象内存占用前RANK名的简报[支持内存追踪过滤]", __indent);
            help("ustat", "[RANK]", "按类型输出引擎对象内存占用前RANK名的简报[支持内存追踪过滤]", __indent);
            help("bar", "[RANK]", "输出IL2CPP类型内存占用前RANK名图形简报[支持内存追踪过滤]", __indent);
            help("ubar", "[RANK]", "输出引擎类型内存占用前RANK名图形简报[支持内存追踪过滤]", __indent);
            help("list", NULL, "列举IL2CPP类型所有活跃对象内存占用简报[支持内存追踪过滤]", __indent);
            help("ulist", NULL, "列举引擎类型所有活跃对象内存占用简报[支持内存追踪过滤]", __indent);
            help("heap", "[RANK]", "输出动态内存简报", __indent);
            help("save", NULL, "把当前内存快照分析结果以sqlite3格式保存到本机", __indent);
            help("uuid", NULL, "查看内存快照UUID", __indent);
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
    cout << "argc=" << argc << endl;
    for (auto i = 0; i < argc; i++)
    {
        cout << "argv[" << i << "]=" << argv[i] << endl;
    }
    
    if (argc > 1)
    {
        processRecord(argv[1]);
    }
    
    return 0;
}
