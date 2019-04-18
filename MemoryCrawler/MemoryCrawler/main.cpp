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
    char *item = new char[64];
    auto iter = 0;
    for (auto i = 0; i < charCount; i++)
    {
        if (command[i] == ' ')
        {
            if (iter > 0)
            {
                auto size = strlen(item);
                auto option = new char[size];
                strcpy(option, item);
                options.push_back(option);
                memset(item, 0, size);
                iter = 0;
            }
        }
        else
        {
            item[iter++] = command[i];
        }
    }
    
    if (strlen(item) > 0) { options.push_back(item); }
    
    callback(options);
    
    for (auto i = 0; i < options.size(); i++) { delete [] options[i]; }
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
                                   }
                                   //todo: comparation
                               });
            delete crawler;
        }
        else if (strbeg(command, "save"))
        {
            SnapshotCrawlerCache().save(mainCrawler);
        }
        else if (strbeg(command, "load"))
        {
            readCommandOptions(command, [](std::vector<const char *> &options)
                               {
                                   if (options.size() == 1) {return;}
                                   MemorySnapshotCrawler crawler(options[1]);
                                   crawler.crawl();
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
                                       auto address = strtoll(options[i], nullptr, 10);
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
                                       auto address = strtoll(options[i], nullptr, 10);
                                       printf("%lld\n", mainCrawler.findMObjectOfNObject(address));
                                   }
                               });
        }
        else if (strbeg(command, "kref"))
        {
            std::cout << "\e[96m";
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = strtoll(options[i], nullptr, 10);
                                       mainCrawler.dumpMRefChain(address, false);
                                   }
                               });
            std::cout << "\e[0m";
        }
        else if (strbeg(command, "ukref"))
        {
            std::cout << "\e[96m";
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = strtoll(options[i], nullptr, 10);
                                       mainCrawler.dumpNRefChain(address, false);
                                   }
                               });
            std::cout << "\e[0m";
        }
        else if (strbeg(command, "ref"))
        {
            std::cout << "\e[96m";
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = strtoll(options[i], nullptr, 10);
                                       mainCrawler.dumpMRefChain(address, true);
                                   }
                               });
            std::cout << "\e[0m";
        }
        else if (strbeg(command, "uref"))
        {
            std::cout << "\e[96m";
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       auto address = strtoll(options[i], nullptr, 10);
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
        else if (strbeg(command, "exit"))
        {
            return;
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
