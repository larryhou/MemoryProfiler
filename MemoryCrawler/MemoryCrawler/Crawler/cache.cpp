//
//  cache.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/9.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "cache.h"

CrawlerCache::CrawlerCache()
{
    
}

void CrawlerCache::open(const char *filepath)
{
    auto rc = sqlite3_open(filepath, &database);
    assert(rc == 0);
}

void CrawlerCache::createTypeTable()
{
    const char *sql = "CREATE TABLE types (" \
        "arrayRank INTEGER" \
        "assembly TEXT NOT NULL" \
        "baseOrElementTypeIndex INTEGER" \
        "isArray INTEGER" \
        "isValueType INTEGER" \
        "name TEXT NOT NULL" \
        "size INTEGER" \
        "staticFieldBytes BLOB" \
        "typeIndex INTEGER PRIMARY KEY" \
        "typeInfoAddress INTEGER" \
        "nativeTypeArrayIndex INTEGER" \
        "fields BLOB";
    char *errmsg;
    sqlite3_exec(database, sql, nullptr, nullptr, &errmsg);
}

void CrawlerCache::createFieldTable()
{
    const char *sql = "CREATE TABLE fields (" \
        "id INTEGER PRIMARY KEY" \
        "isStatic INTEGER" \
        "name TEXT NOT NULL" \
        "offset INTEGER" \
        "typeIndex INTEGER REFERENCES types (typeIndex)";
    char *errmsg;
    sqlite3_exec(database, sql, nullptr, nullptr, &errmsg);
}

void CrawlerCache::insert(Array<TypeDescription> &types)
{
    char buffer[32*1024];
    
    char *errmsg;
    sqlite3_stmt *stmt;
    
    sqlite3_exec(database, "BEGIN TRANSACTION", nullptr, nullptr, &errmsg);
    {
        char sql[] = "INSERT INTO types VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12)";
        sqlite3_prepare_v2(database, sql, (int)strlen(sql), &stmt, nullptr);
    }
    
    for (auto i = 0; i < types.size; i++)
    {
        auto &t = types[i];
        char *iter = buffer;
        for (auto n = 0; n < t.fields->size; n++)
        {
            auto &f = t.fields->items[n];
            new(iter) int64_t(f.typeIndex << 16 | n);
            iter += 8;
        }
        sqlite3_bind_int(stmt, 1, t.arrayRank);
        sqlite3_bind_text(stmt, 2, t.assembly->c_str(), (int)t.assembly->size(), SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, t.baseOrElementTypeIndex);
        sqlite3_bind_int(stmt, 4, t.isArray);
        sqlite3_bind_int(stmt, 5, t.isValueType);
        sqlite3_bind_text(stmt, 6, t.name->c_str(), (int)t.name->size(), SQLITE_STATIC);
        sqlite3_bind_int(stmt, 7, t.size);
        sqlite3_bind_blob(stmt, 8, t.staticFieldBytes->items, t.staticFieldBytes->size, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 9, t.typeIndex);
        sqlite3_bind_int64(stmt, 10, t.typeInfoAddress);
        sqlite3_bind_int64(stmt, 11, t.nativeTypeArrayIndex);
        sqlite3_bind_blob(stmt, 12, buffer, (int)(iter - buffer), SQLITE_STATIC);
        if (sqlite3_step(stmt) != SQLITE_DONE) {}
        sqlite3_reset(stmt);
    }
    
    sqlite3_exec(database, "COMMIT TRANSACTION", nullptr, nullptr, &errmsg);
    sqlite3_finalize(stmt);
    
    // type fields
    sqlite3_exec(database, "BEGIN TRANSACTION", nullptr, nullptr, &errmsg);
    {
        char sql[] = "INSERT INTO fields VALUES (?1, ?2, ?3, ?4, ?5)";
        sqlite3_prepare_v2(database, sql, (int)strlen(sql), &stmt, nullptr);
    }
    
    for (auto i = 0; i < types.size; i++)
    {
        auto &t = types[i];
        for (auto n = 0; n < t.fields->size; n++)
        {
            auto &f = t.fields->items[n];
            sqlite3_bind_int64(stmt, 1, t.typeIndex << 16 | n);
            sqlite3_bind_int(stmt, 2, f.isStatic);
            sqlite3_bind_text(stmt, 3, t.name->c_str(), (int)t.name->size(), SQLITE_STATIC);
            sqlite3_bind_int(stmt, 4, f.offset);
            sqlite3_bind_int(stmt, 5, f.typeIndex);
            if (sqlite3_step(stmt) != SQLITE_DONE) {}
            sqlite3_reset(stmt);
        }
    }
    
    sqlite3_exec(database, "COMMIT TRANSACTION", nullptr, nullptr, &errmsg);
    sqlite3_finalize(stmt);
}

void CrawlerCache::createJointTable()
{
    const char *sql = "CREATE TABLE joints (" \
        "jointArrayIndex INTEGER NOT NULL PRIMARY KEY," \
        "hookTypeIndex INTEGER," \
        "hookObjectIndex INTEGER," \
        "hookObjectAddress INTEGER," \
        "fieldTypeIndex INTEGER," \
        "fieldSlotIndex INTEGER," \
        "fieldOffset INTEGER," \
        "fieldAddress INTEGER," \
        "elementArrayIndex INTEGER," \
        "gcHandleIndex INTEGER," \
        "isStatic INTEGER);";
    char *errmsg;
    sqlite3_exec(database, sql, nullptr, nullptr, &errmsg);
}

void CrawlerCache::insert(InstanceManager<EntityJoint> &joints)
{
    char *errmsg;
    sqlite3_stmt *stmt;
    
    sqlite3_exec(database, "BEGIN TRANSACTION", nullptr, nullptr, &errmsg);
    {
        char sql[] = "INSERT INTO joints VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)";
        sqlite3_prepare_v2(database, sql, (int)strlen(sql), &stmt, nullptr);
    }
    
    for (auto i = 0; i < joints.size(); i++)
    {
        auto &ej = joints[i];
        sqlite3_bind_int(stmt, 1, ej.jointArrayIndex);
        sqlite3_bind_int(stmt, 2, ej.hookTypeIndex);
        sqlite3_bind_int(stmt, 3, ej.hookObjectIndex);
        sqlite3_bind_int64(stmt, 4, ej.hookObjectAddress);
        sqlite3_bind_int(stmt, 5, ej.fieldTypeIndex);
        sqlite3_bind_int(stmt, 6, ej.fieldSlotIndex);
        sqlite3_bind_int(stmt, 7, ej.fieldOffset);
        sqlite3_bind_int64(stmt, 8, ej.fieldAddress);
        sqlite3_bind_int(stmt, 9, ej.elementArrayIndex);
        sqlite3_bind_int(stmt, 10, ej.gcHandleIndex);
        sqlite3_bind_int(stmt, 11, ej.isStatic);
        if (sqlite3_step(stmt) != SQLITE_DONE) {}
        sqlite3_reset(stmt);
    }
    
    sqlite3_exec(database, "COMMIT TRANSACTION", nullptr, nullptr, &errmsg);
    sqlite3_finalize(stmt);
}

void CrawlerCache::createConnectionTable()
{
    const char *sql = "CREATE TABLE connections (" \
        "id INTEGER NOT NULL PRIMARY KEY" \
        "from INTEGER" \
        "fromKind INTEGER" \
        "to INTEGER" \
        "toKind INTEGER" \
        "jointArrayIndex INTEGER REFERENCES joints (jointArrayIndex)";
    char *errmsg;
    sqlite3_exec(database, sql, nullptr, nullptr, &errmsg);
}

void CrawlerCache::insert(InstanceManager<EntityConnection> &connections)
{
    char *errmsg;
    sqlite3_stmt *stmt;
    
    sqlite3_exec(database, "BEGIN TRANSACTION", nullptr, nullptr, &errmsg);
    {
        char sql[] = "INSERT INTO connections VALUES (?1, ?2, ?3, ?4, ?5, ?6)";
        sqlite3_prepare_v2(database, sql, (int)strlen(sql), &stmt, nullptr);
    }
    
    for (auto i = 0; i < connections.size(); i++)
    {
        auto &ec = connections[i];
        sqlite3_bind_int(stmt, 1, i);
        sqlite3_bind_int(stmt, 2, ec.from);
        sqlite3_bind_int(stmt, 3, ec.fromKind);
        sqlite3_bind_int(stmt, 4, ec.to);
        sqlite3_bind_int(stmt, 5, ec.toKind);
        sqlite3_bind_int(stmt, 6, ec.jointArrayIndex);
        if (sqlite3_step(stmt) != SQLITE_DONE) {}
        sqlite3_reset(stmt);
    }
    
    sqlite3_exec(database, "COMMIT TRANSACTION", nullptr, nullptr, &errmsg);
    sqlite3_finalize(stmt);
}

void CrawlerCache::createObjectTable()
{
    const char *sql = "CREATE TABLE objects (" \
    "address INTEGER NOT NULL" \
    "typeIndex INTEGER REFERENCES types (typeIndex)" \
    "managedObjectIndex INTEGER NOT NULL PRIMARY KEY" \
    "nativeObjectIndex INTEGER" \
    "gcHandleIndex INTEGER" \
    "isValueType INTEGER" \
    "size INTEGER" \
    "nativeSize INTEGER" \
    "jointArrayIndex INTEGER REFERENCES joints (jointArrayIndex)";
    char *errmsg;
    sqlite3_exec(database, sql, nullptr, nullptr, &errmsg);
}

void CrawlerCache::insert(InstanceManager<ManagedObject> &objects)
{
    char *errmsg;
    sqlite3_stmt *stmt;
    
    sqlite3_exec(database, "BEGIN TRANSACTION", nullptr, nullptr, &errmsg);
    {
        char sql[] = "INSERT INTO objects VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)";
        sqlite3_prepare_v2(database, sql, (int)strlen(sql), &stmt, nullptr);
    }
    
    for (auto i = 0; i < objects.size(); i++)
    {
        auto &ec = objects[i];
        sqlite3_bind_int64(stmt, 1, ec.address);
        sqlite3_bind_int(stmt, 2, ec.typeIndex);
        sqlite3_bind_int(stmt, 3, ec.managedObjectIndex);
        sqlite3_bind_int(stmt, 4, ec.nativeObjectIndex);
        sqlite3_bind_int(stmt, 5, ec.gcHandleIndex);
        sqlite3_bind_int(stmt, 6, ec.isValueType);
        sqlite3_bind_int(stmt, 7, ec.size);
        sqlite3_bind_int(stmt, 8, ec.nativeSize);
        sqlite3_bind_int(stmt, 9, ec.jointArrayIndex);
        if (sqlite3_step(stmt) != SQLITE_DONE) {}
        sqlite3_reset(stmt);
    }
    
    sqlite3_exec(database, "COMMIT TRANSACTION", nullptr, nullptr, &errmsg);
    sqlite3_finalize(stmt);
}

//MemorySnapshotCrawler &CrawlerCache::read()
//{
//
//}

void CrawlerCache::save(MemorySnapshotCrawler &crawler)
{
    char filepath[64];
    sprintf(filepath, "__cpp_cache/%s.db", crawler.snapshot.uuid->c_str());
    open(filepath);
    
    createTypeTable();
    createFieldTable();
    insert(*crawler.snapshot.typeDescriptions);
    
    createJointTable();
    insert(crawler.joints);
    
    createConnectionTable();
    insert(crawler.connections);
    
    createObjectTable();
    insert(crawler.managedObjects);
    
    sqlite3_close(database);
}

CrawlerCache::~CrawlerCache()
{
    
}
