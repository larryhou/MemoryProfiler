//
//  cache.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/9.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef cache_h
#define cache_h

#include <stdio.h>
#include <sqlite3.h>
#include <new>
#include "crawler.h"
#include "perf.h"

class CrawlerCache
{
    TimeSampler<std::nano> sampler;
    sqlite3 *database;
    
public:
    CrawlerCache();
    void open(const char *filepath);
    void save(MemorySnapshotCrawler &crawler);
    MemorySnapshotCrawler &read();
    ~CrawlerCache();
    
private:
    void createTypeTable();
    void createFieldTable();
    void insert(Array<TypeDescription> &types);
    void createJointTable();
    void insert(InstanceManager<EntityJoint> &joints);
    void createConnectionTable();
    void insert(InstanceManager<EntityConnection> &connections);
    void createObjectTable();
    void insert(InstanceManager<ManagedObject> &objects);
};

#endif /* cache_h */
