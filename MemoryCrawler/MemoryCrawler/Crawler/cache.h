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

class SnapshotCrawlerCache
{
    char __buffer[32*1024];
    TimeSampler<std::nano> __sampler;
    sqlite3 *__database = nullptr;
    const char *__workspace = "__cpp_cache";
    
public:
    SnapshotCrawlerCache();
    void open(const char *filepath);
    void save(MemorySnapshotCrawler &crawler);
    void read(const char *uuid, MemorySnapshotCrawler *crawler);
    ~SnapshotCrawlerCache();
    
private:
    void createNativeTypeTable();
    void insert(Array<PackedNativeType> &nativeTypes);
    void createNativeObjectTable();
    void insert(Array<PackedNativeUnityEngineObject> &nativeObjects);
    void createTypeTable();
    void createFieldTable();
    void insert(Array<TypeDescription> &types);
    void createJointTable();
    void insert(InstanceManager<EntityJoint> &joints);
    void createConnectionTable();
    void insert(InstanceManager<EntityConnection> &connections);
    void createObjectTable();
    void insert(InstanceManager<ManagedObject> &objects);
    
    void createVMTable();
    void insertVMTable(VirtualMachineInformation &vm);
    
    void createStringTable();
    void insertStringTable(MemorySnapshotCrawler &crawler);
    
    template <typename T>
    void insert(const char * sql, InstanceManager<T> &manager, std::function<void(T &item, sqlite3_stmt *stmt)> eachcall);
    
    template <typename T>
    void select(const char * sql, int32_t size, InstanceManager<T> &manager, std::function<void(T &item, sqlite3_stmt *stmt)> eachcall);
    
    template <typename T>
    void insert(const char * sql, Array<T> &array, std::function<void(T &item, sqlite3_stmt *stmt)> eachcall);
    
    template <typename T>
    void select(const char * sql, Array<T> &array, std::function<void(T &item, sqlite3_stmt *stmt)> eachcall);
    
    void select(const char * sql, int32_t size, std::function<void(sqlite3_stmt *stmt)> eachcall);
    
    void create(const char *sql);
    int32_t selectCount(const char *name);
    
    void removeRedundants(MemorySnapshotCrawler &crawler);
};

template <typename T>
void SnapshotCrawlerCache::insert(const char * sql, Array<T> &array, std::function<void(T &item, sqlite3_stmt *stmt)> eachcall)
{
    char *errmsg;
    sqlite3_stmt *stmt;
    
    sqlite3_exec(__database, "BEGIN TRANSACTION", nullptr, nullptr, &errmsg);
    sqlite3_prepare_v2(__database, sql, (int)strlen(sql), &stmt, nullptr);
    
    for (auto i = 0; i < array.size; i++)
    {
        eachcall(array[i], stmt);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {}
        sqlite3_reset(stmt);
    }
    
    sqlite3_exec(__database, "COMMIT TRANSACTION", nullptr, nullptr, &errmsg);
    sqlite3_finalize(stmt);
}

template <typename T>
void SnapshotCrawlerCache::select(const char * sql, Array<T> &array, std::function<void(T &item, sqlite3_stmt *stmt)> eachcall)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(__database, sql, (int)strlen(sql), &stmt, nullptr);
    
    for (auto i = 0; i < array.size; i++)
    {
        if (sqlite3_step(stmt) != SQLITE_DONE) {}
        eachcall(array[i], stmt);
    }
    
    sqlite3_finalize(stmt);
}

template <typename T>
void SnapshotCrawlerCache::insert(const char * sql, InstanceManager<T> &manager, std::function<void(T &item, sqlite3_stmt *stmt)> eachcall)
{
    char *errmsg;
    sqlite3_stmt *stmt;
    
    sqlite3_exec(__database, "BEGIN TRANSACTION", nullptr, nullptr, &errmsg);
    sqlite3_prepare_v2(__database, sql, (int)strlen(sql), &stmt, nullptr);
    
    for (auto i = 0; i < manager.size(); i++)
    {
        eachcall(manager[i], stmt);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {}
        sqlite3_reset(stmt);
    }
    
    sqlite3_exec(__database, "COMMIT TRANSACTION", nullptr, nullptr, &errmsg);
    sqlite3_finalize(stmt);
}

template <typename T>
void SnapshotCrawlerCache::select(const char * sql, int32_t size, InstanceManager<T> &manager, std::function<void(T &item, sqlite3_stmt *stmt)> eachcall)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(__database, sql, (int)strlen(sql), &stmt, nullptr);
    
    for (auto i = 0; i < size; i++)
    {
        if (sqlite3_step(stmt) != SQLITE_DONE) {}
        eachcall(manager.add(), stmt);
    }
    
    sqlite3_finalize(stmt);
}


#endif /* cache_h */
