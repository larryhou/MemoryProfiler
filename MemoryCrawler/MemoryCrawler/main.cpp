//
//  main.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/2.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include <thread>
#include <istream>
#include <vector>
#include <ratio>
#include "Crawler/snapshot.h"
#include "Crawler/stream.h"
#include "Crawler/perf.h"
#include "Crawler/serialize.h"
#include "Crawler/crawler.h"
#include "Crawler/cache.h"
#include "Crawler/args.h"

using std::cout;
using std::endl;
using std::vector;

void readCachedSnapshot(const char *uuid);

inline void crawlSnapshot(const char * filepath)
{
    TimeSampler<std::nano> sampler;
    
    sampler.begin("testStream");
    sampler.begin("read_snapshot");
    MemorySnapshotReader reader;
    auto &snapshot = reader.read(filepath);
    sampler.end();
    
    sampler.begin("craw_snapshot");
    MemorySnapshotCrawler crawler(snapshot);
    crawler.crawl();
    sampler.end();
    sampler.end();
    sampler.summary();
    
    SnapshotCrawlerCache cache;
    cache.save(crawler);
    
    char command[256];
    while (true)
    {
        std::cout << "/> ";
        
        memset(command, 0, sizeof(command));
        while (strlen(command) == 0)
        {
            std::cin.getline(command, sizeof(command));
        }
        
        printf(">> %s\n", command);
        if (0 == strcmp(command, "read"))
        {
            readCachedSnapshot(crawler.snapshot.uuid->c_str());
        }
        else if (0 == strcmp(command, "ref"))
        {
            
        }
        else if (0 == strcmp(command, "uref"))
        {
            
        }
        else
        {
            printf("not supported command [%s]\n", command);
        }
    }
}

void readCachedSnapshot(const char *uuid)
{
    SnapshotCrawlerCache cache;
    MemorySnapshotCrawler &serializedCrawler = cache.read(uuid);
    cout << serializedCrawler.managedObjects.size() << endl;
    delete &serializedCrawler;
}

int main(int argc, const char * argv[])
{
    ArgumentManager manager;
    manager.addOption(new ArgumentOption("--file-snapshot-path", "-f", "内存快照路径", true, true));
    manager.addOption(new ArgumentOption("--comp-snapshot-path", "-c", "参与比对的内存快照路径", true));
    manager.addOption(new ArgumentOption("--debug", "-d", "开启调试模式", false));
    manager.addOption(new ArgumentOption("--interactive", "-i", "开启交互模式", false));
    manager.help();
    manager.parseArguments(argc - 1, &argv[1]);
    
    cout << "argc=" << argc << endl;
    for (auto i = 0; i < argc; i++)
    {
        cout << "argv[" << i << "]=" << argv[i] << endl;
    }
    
    if (argc > 1)
    {
        crawlSnapshot(argv[1]);
    }
    
    return 0;
}
