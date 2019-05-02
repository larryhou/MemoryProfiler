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

using std::cout;
using std::endl;
using std::vector;

void readCachedSnapshot(const char *uuid);

bool strbeg(const char *str, const char *cmp)
{
    auto size = strlen(cmp);
    for (auto i = 0; i < size; i++)
    {
        if (str[i] != cmp[i]){return false;}
    }
    if (strlen(str) > size)
    {
        return str[size] == ' ';
    }
    return true;
}

void readCommandOptions(const char *command, std::function<void(std::vector<const char *> &)> callback)
{
    std::vector<const char *> options;
    auto charCount = strlen(command);
    char *item = new char[256];
    auto iter = 0;
    for (auto i = 0; i < charCount; i++)
    {
        if (command[i] == ' ')
        {
            if (iter > 0)
            {
                item[iter] = 0; // end c string
                auto size = strlen(item);
                auto option = new char[size];
                strcpy(option, item);
                options.push_back(option);
                iter = 0;
            }
        }
        else
        {
            item[iter++] = command[i];
        }
    }
    
    item[iter] = 0; // end c string
    if (strlen(item) > 0) { options.push_back(item); }
    
    callback(options);
    
    for (auto i = 0; i < options.size(); i++) { delete [] options[i]; }
}

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

#include <unistd.h>
#include <memory>
using std::ofstream;
void processSnapshot(const char * filepath)
{
    MemorySnapshotCrawler mainCrawler(filepath);
    mainCrawler.crawl();
    
    char cmdpath[128];
    mkdir("__commands", 0777);
    memset(cmdpath, 0, sizeof(cmdpath));
    sprintf(cmdpath, "__commands/%s.clog", mainCrawler.snapshot.uuid.c_str());
    
    ofstream clog;
    clog.open(cmdpath, ofstream::app);
    
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
            replaying ? usleep(500000) : usleep(100000);
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
        else if (strbeg(command, "info"))
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
        else if (strbeg(command, "run"))
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
        else if (strbeg(command, "exit"))
        {
            recordable = false;
            printf("\e[0m");
            clog.close();
            return;
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
        }
        
        if (!replaying && recordable)
        {
            clog << input.c_str() << endl;
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
        processSnapshot(argv[1]);
    }
    
    return 0;
}
