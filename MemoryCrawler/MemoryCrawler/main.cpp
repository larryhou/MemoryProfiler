//
//  main.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/2.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include <string>
#include <vector>
#include <stdlib.h>
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
void processSnapshot(const char * filepath)
{
    MemorySnapshotCrawler mainCrawler(filepath);
    mainCrawler.crawl();
    
    char uuid[40];
    std::strcpy(uuid, mainCrawler.snapshot.uuid.c_str());
    
    std::istream *stream = &std::cin;
    while (true)
    {
        auto original = typeid(*stream) == typeid(std::cin);
        
        std::cout << "\e[93m/> ";
        std::string input;
        while (!getline(*stream, input))
        {
            if (!original)
            {
                ((ifstream *)stream)->close();
                stream = &std::cin;
                original = true;
            }
            stream->clear();
            usleep(100000);
        }
        
        if (!original) {printf("%s\n", input.c_str());}
        std::cout << "\e[0m";
        
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
            std::cout << "\e[96m";
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       mainCrawler.dumpMRefChain(address, false, -1);
                                   }
                               });
            std::cout << "\e[0m";
        }
        else if (strbeg(command, "UKREF")) // complete native object reference chains except circular ones
        {
            std::cout << "\e[96m";
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       mainCrawler.dumpNRefChain(address, false, -1);
                                   }
                               });
            std::cout << "\e[0m";
        }
        else if (strbeg(command, "REF")) // complete managed object reference chains
        {
            std::cout << "\e[96m";
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       mainCrawler.dumpMRefChain(address, true, -1);
                                   }
                               });
            std::cout << "\e[0m";
        }
        else if (strbeg(command, "UREF")) // complete native object reference chains
        {
            std::cout << "\e[96m";
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       mainCrawler.dumpNRefChain(address, true, -1);
                                   }
                               });
            std::cout << "\e[0m";
        }
        else if (strbeg(command, "kref")) // partial managed object reference chains except circular ones
        {
            std::cout << "\e[96m";
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       mainCrawler.dumpMRefChain(address, false);
                                   }
                               });
            std::cout << "\e[0m";
        }
        else if (strbeg(command, "ukref")) // partial native object reference chains except circular ones
        {
            std::cout << "\e[96m";
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       mainCrawler.dumpNRefChain(address, false);
                                   }
                               });
            std::cout << "\e[0m";
        }
        else if (strbeg(command, "ref")) // partial managed object reference chains
        {
            std::cout << "\e[96m";
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       mainCrawler.dumpMRefChain(address, true);
                                   }
                               });
            std::cout << "\e[0m";
        }
        else if (strbeg(command, "uref")) // partial native object reference chains
        {
            std::cout << "\e[96m";
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = castAddress(options[i]);
                                       mainCrawler.dumpNRefChain(address, true);
                                   }
                               });
            std::cout << "\e[0m";
        }
        else if (strbeg(command, "run"))
        {
            std::vector<string> commands;
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() == 1){return;}
                                   auto ifs = new ifstream;
                                   ifs->open(options[1]);
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
            mainCrawler.trackingMode = !mainCrawler.trackingMode;
            if (mainCrawler.trackingMode)
            {
                printf("\e[7mENTER LEAK TRACKING MODE\e[0m\n");
            }
            else
            {
                printf("\e[32m\e[7mLEAVE LEAK TRACKING MODE\e[0m\n");
            }
        }
        else if (strbeg(command, "list"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   if (options.size() == 1)
                                   {
                                       mainCrawler.trackMObjects(CS_new);
                                   }
                                   else
                                   {
                                       const char *subcommand = options[1];
                                       if (0 == strcmp(subcommand, "new"))
                                       {
                                           mainCrawler.trackMObjects(CS_new);
                                       }
                                       else if (0 == strcmp(subcommand, "leak"))
                                       {
                                           mainCrawler.trackMObjects(CS_identical);
                                       }
                                   }
                               });
        }
        else if (strbeg(command, "ulist"))
        {
            readCommandOptions(command, [&](std::vector<const char *> options)
                               {
                                   if (options.size() == 1)
                                   {
                                       mainCrawler.trackNObjects(CS_new);
                                   }
                                   else
                                   {
                                       const char *subcommand = options[1];
                                       int32_t depth = options.size() > 2 ? atoi(options[2]) : 5;
                                       if (0 == strcmp(subcommand, "new"))
                                       {
                                           mainCrawler.trackNObjects(CS_new, depth);
                                       }
                                       else if (0 == strcmp(subcommand, "leak"))
                                       {
                                           mainCrawler.trackNObjects(CS_identical, depth);
                                       }
                                   }
                               });
        }
        else if (strbeg(command, "sum"))
        {
            
        }
        else if (strbeg(command, "usum"))
        {
            
        }
        else if (strbeg(command, "exit"))
        {
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
