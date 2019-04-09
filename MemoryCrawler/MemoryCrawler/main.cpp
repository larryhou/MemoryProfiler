//
//  main.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/2.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include <thread>
#include <iostream>
#include <vector>
#include <ratio>
#include "Crawler/snapshot.h"
#include "Crawler/stream.h"
#include "Crawler/perf.h"
#include "Crawler/serialize.h"
#include "Crawler/crawler.h"
#include "Crawler/cache.h"

using std::cout;
using std::endl;
using std::vector;

inline void testStream(const char * filepath)
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
}

int main(int argc, const char * argv[])
{
    TimeSampler<> profiler;
    
    cout << "argc=" << argc << endl;
    for (auto i = 0; i < argc; i++)
    {
        cout << "argv[" << i << "]=" << argv[i] << endl;
    }
    
    if (argc > 1)
    {
        testStream(argv[1]);
    }
    
    return 0;
}
