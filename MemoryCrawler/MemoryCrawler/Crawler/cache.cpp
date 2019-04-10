//
//  cache.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/9.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "cache.h"
#include <sys/stat.h>

SnapshotCrawlerCache::SnapshotCrawlerCache()
{
    __sampler.begin("SnapshotCrawlerCache");
}

void SnapshotCrawlerCache::open(const char *filepath)
{
    auto rc = sqlite3_open(filepath, &__database);
    assert(rc == 0);
    
    sqlite3_exec(__database, "PRAGMA FOREIGN_KEYS=OFF;", nullptr, nullptr, nullptr);
    sqlite3_exec(__database, "PRAGMA journal_mode = MEMORY;", nullptr, nullptr, nullptr);
}

void SnapshotCrawlerCache::create(const char *sql)
{
    char *errmsg;
    sqlite3_exec(__database, sql, nullptr, nullptr, &errmsg);
}

int sqliteCallbackCount(void *context, int argc, char **argv, char **columns)
{
    auto ptr = (int *)context;
    *ptr = *(int *)argv[0];
    return 0;
}

int32_t SnapshotCrawlerCache::count(const char *name)
{
    sprintf(__buffer, "select count(*) from %s;", name);
    int count = 0;
    sqlite3_exec(__database, __buffer, sqliteCallbackCount, &count, nullptr);
    return count;
}

void SnapshotCrawlerCache::createNativeTypeTable()
{
    create("CREATE TABLE nativeTypes (" \
           "typeIndex INTEGER PRIMARY KEY," \
           "name TEXT NOT NULL," \
           "nativeBaseTypeArrayIndex INTEGER," \
           "managedTypeArrayIndex INTEGER);");
}

void SnapshotCrawlerCache::insert(Array<PackedNativeType> &nativeTypes)
{
    insert<PackedNativeType>("INSERT INTO nativeTypes VALUES (?1, ?2, ?3, ?4);",
                             nativeTypes, [](PackedNativeType &nt, sqlite3_stmt *stmt)
                             {
                                 sqlite3_bind_int(stmt, 1, nt.typeIndex);
                                 sqlite3_bind_text(stmt, 2, nt.name->c_str(), (int)nt.name->size(), SQLITE_STATIC);
                                 sqlite3_bind_int64(stmt, 3, nt.nativeBaseTypeArrayIndex);
                                 sqlite3_bind_int(stmt, 4, nt.managedTypeArrayIndex);
                             });
}

void SnapshotCrawlerCache::createNativeObjectTable()
{
    create("CREATE TABLE nativeObjects (" \
           "hideFlags INTEGER," \
           "instanceId INTEGER," \
           "isDontDestroyOnLoad INTEGER," \
           "isManager INTEGER," \
           "isPersistent INTEGER," \
           "name TEXT NOT NULL," \
           "nativeObjectAddress INTEGER," \
           "nativeTypeArrayIndex INTEGER REFERENCES nativeTypes (typeIndex)," \
           "size INTEGER," \
           "managedObjectArrayIndex INTEGER," \
           "nativeObjectArrayIndex INTEGER PRIMARY KEY);");
}

void SnapshotCrawlerCache::insert(Array<PackedNativeUnityEngineObject> &nativeObjects)
{
    insert<PackedNativeUnityEngineObject>("INSERT INTO nativeObjects VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11);",
                                          nativeObjects, [](PackedNativeUnityEngineObject &no, sqlite3_stmt *stmt)
                                          {
                                              sqlite3_bind_int(stmt, 1, no.hideFlags);
                                              sqlite3_bind_int(stmt, 2, no.instanceId);
                                              sqlite3_bind_int(stmt, 3, no.isDontDestroyOnLoad);
                                              sqlite3_bind_int(stmt, 4, no.isManager);
                                              sqlite3_bind_int(stmt, 5, no.isPersistent);
                                              sqlite3_bind_text(stmt, 6, no.name->c_str(), (int)no.name->size(), SQLITE_STATIC);
                                              sqlite3_bind_int64(stmt, 7, no.nativeObjectAddress);
                                              sqlite3_bind_int(stmt, 8, no.nativeTypeArrayIndex);
                                              sqlite3_bind_int(stmt, 9, no.size);
                                              sqlite3_bind_int(stmt, 10, no.managedObjectArrayIndex);
                                              sqlite3_bind_int(stmt, 11, no.nativeObjectArrayIndex);
                                          });
}

void SnapshotCrawlerCache::createTypeTable()
{
    create("CREATE TABLE types (" \
           "arrayRank INTEGER," \
           "assembly TEXT NOT NULL," \
           "baseOrElementTypeIndex INTEGER," \
           "isArray INTEGER," \
           "isValueType INTEGER," \
           "name TEXT NOT NULL," \
           "size INTEGER," \
           "staticFieldBytes BLOB," \
           "typeIndex INTEGER PRIMARY KEY," \
           "typeInfoAddress INTEGER," \
           "nativeTypeArrayIndex INTEGER," \
           "fields BLOB);");
}

void SnapshotCrawlerCache::createFieldTable()
{
    create("CREATE TABLE fields (" \
           "id INTEGER PRIMARY KEY," \
           "isStatic INTEGER," \
           "name TEXT NOT NULL," \
           "offset INTEGER," \
           "typeIndex INTEGER REFERENCES types (typeIndex));");
}

void SnapshotCrawlerCache::insert(Array<TypeDescription> &types)
{
    insert<TypeDescription>("INSERT INTO types VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12);",
                            types, [this](TypeDescription &t, sqlite3_stmt *stmt)
                            {
                                char *iter = __buffer;
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
                                if (t.staticFieldBytes != nullptr)
                                {
                                    sqlite3_bind_blob(stmt, 8, t.staticFieldBytes->items, t.staticFieldBytes->size, SQLITE_STATIC);
                                }
                                sqlite3_bind_int(stmt, 9, t.typeIndex);
                                sqlite3_bind_int64(stmt, 10, t.typeInfoAddress);
                                sqlite3_bind_int64(stmt, 11, t.nativeTypeArrayIndex);
                                sqlite3_bind_blob(stmt, 12, __buffer, (int)(iter - __buffer), SQLITE_STATIC);
                            });
    
    // type fields
    insert<TypeDescription>("INSERT INTO fields VALUES (?1, ?2, ?3, ?4, ?5);",
                            types, [](TypeDescription &t, sqlite3_stmt *stmt)
                            {
                                for (auto n = 0; n < t.fields->size; n++)
                                {
                                    auto &f = t.fields->items[n];
                                    sqlite3_bind_int64(stmt, 1, t.typeIndex << 16 | n);
                                    sqlite3_bind_int(stmt, 2, f.isStatic);
                                    sqlite3_bind_text(stmt, 3, t.name->c_str(), (int)t.name->size(), SQLITE_STATIC);
                                    sqlite3_bind_int(stmt, 4, f.offset);
                                    sqlite3_bind_int(stmt, 5, f.typeIndex);
                                }
                            });
}

void SnapshotCrawlerCache::createJointTable()
{
    create("CREATE TABLE joints (" \
           "jointArrayIndex INTEGER PRIMARY KEY," \
           "hookTypeIndex INTEGER," \
           "hookObjectIndex INTEGER," \
           "hookObjectAddress INTEGER," \
           "fieldTypeIndex INTEGER," \
           "fieldSlotIndex INTEGER," \
           "fieldOffset INTEGER," \
           "fieldAddress INTEGER," \
           "elementArrayIndex INTEGER," \
           "gcHandleIndex INTEGER," \
           "isStatic INTEGER);");
}

void SnapshotCrawlerCache::insert(InstanceManager<EntityJoint> &joints)
{
    insert<EntityJoint>("INSERT INTO joints VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11);",
                        joints, [](EntityJoint &ej, sqlite3_stmt *stmt)
                        {
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
                        });
}

void SnapshotCrawlerCache::createConnectionTable()
{
    create("CREATE TABLE connections (" \
           "connectionArrayIndex INTEGER PRIMARY KEY," \
           "fromIndex INTEGER," \
           "fromKind INTEGER," \
           "toIndex INTEGER," \
           "toKind INTEGER," \
           "jointArrayIndex INTEGER REFERENCES joints (jointArrayIndex));");
}

void SnapshotCrawlerCache::insert(InstanceManager<EntityConnection> &connections)
{
    insert<EntityConnection>("INSERT INTO connections VALUES (?1, ?2, ?3, ?4, ?5, ?6);",
                             connections, [](EntityConnection &ec, sqlite3_stmt *stmt)
                             {
                                 sqlite3_bind_int(stmt, 1, ec.connectionArrayIndex);
                                 sqlite3_bind_int(stmt, 2, ec.from);
                                 sqlite3_bind_int(stmt, 3, (int)ec.fromKind);
                                 sqlite3_bind_int(stmt, 4, ec.to);
                                 sqlite3_bind_int(stmt, 5, (int)ec.toKind);
                                 sqlite3_bind_int(stmt, 6, ec.jointArrayIndex);
                             });
}

void SnapshotCrawlerCache::createObjectTable()
{
    create("CREATE TABLE objects (" \
           "address INTEGER," \
           "typeIndex INTEGER REFERENCES types (typeIndex)," \
           "managedObjectIndex INTEGER PRIMARY KEY," \
           "nativeObjectIndex INTEGER," \
           "gcHandleIndex INTEGER," \
           "isValueType INTEGER," \
           "size INTEGER," \
           "nativeSize INTEGER," \
           "jointArrayIndex INTEGER REFERENCES joints (jointArrayIndex));");
}

void SnapshotCrawlerCache::insert(InstanceManager<ManagedObject> &objects)
{
    insert<ManagedObject>("INSERT INTO objects VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9);",
                          objects, [](ManagedObject &mo, sqlite3_stmt *stmt)
                          {
                              sqlite3_bind_int64(stmt, 1, mo.address);
                              sqlite3_bind_int(stmt, 2, mo.typeIndex);
                              sqlite3_bind_int(stmt, 3, mo.managedObjectIndex);
                              sqlite3_bind_int(stmt, 4, mo.nativeObjectIndex);
                              sqlite3_bind_int(stmt, 5, mo.gcHandleIndex);
                              sqlite3_bind_int(stmt, 6, mo.isValueType);
                              sqlite3_bind_int(stmt, 7, mo.size);
                              sqlite3_bind_int(stmt, 8, mo.nativeSize);
                              sqlite3_bind_int(stmt, 9, mo.jointArrayIndex);
                          });
}

MemorySnapshotCrawler &SnapshotCrawlerCache::read(const char *uuid)
{
    char filepath[64];
    sprintf(filepath, "%s/%s.db", __workspace, uuid);
    remove(filepath);
    
    __sampler.begin("open");
    open(filepath);
    __sampler.end();
    
    auto snapshot = new PackedMemorySnapshot;
    
    snapshot->nativeTypes = new Array<PackedNativeType>(count("nativeTypes"));
    snapshot->nativeObjects = new Array<PackedNativeUnityEngineObject>(count("nativeObjects"));
    snapshot->typeDescriptions = new Array<TypeDescription>(count("types"));
    
    auto crawler = new MemorySnapshotCrawler(*snapshot);
    
    
    return *crawler;
}

void SnapshotCrawlerCache::save(MemorySnapshotCrawler &crawler)
{
    __sampler.begin("SnapshotCrawlerCache::save");
    mkdir(__workspace, 0777);
    
    char filepath[64];
    sprintf(filepath, "%s/%s.db", __workspace, crawler.snapshot.uuid->c_str());
    remove(filepath);
    
    __sampler.begin("open");
    open(filepath);
    __sampler.end();
    
    __sampler.begin("create_native_types");
    createNativeTypeTable();
    __sampler.end();
    
    __sampler.begin("create_native_objects");
    createNativeObjectTable();
    __sampler.end();
    
    __sampler.begin("create_managed_types");
    createTypeTable();
    __sampler.end();
    
    __sampler.begin("create_type_fields");
    createFieldTable();
    __sampler.end();
    
    __sampler.begin("create_joints");
    createJointTable();
    __sampler.end();
    
    __sampler.begin("create_objects");
    createObjectTable();
    __sampler.end();
    
    __sampler.begin("create_connections");
    createConnectionTable();
    __sampler.end();
    
    __sampler.begin("insert_native_types");
    insert(*crawler.snapshot.nativeTypes);
    __sampler.end();
    
    __sampler.begin("insert_native_objects");
    insert(*crawler.snapshot.nativeObjects);
    __sampler.end();
    
    __sampler.begin("insert_managed_types");
    insert(*crawler.snapshot.typeDescriptions);
    __sampler.end();
    
    __sampler.begin("insert_joints");
    insert(crawler.joints);
    __sampler.end();
    
    __sampler.begin("insert_objects");
    insert(crawler.managedObjects);
    __sampler.end();
    
    __sampler.begin("insert_connections");
    insert(crawler.connections);
    __sampler.end();
    
    __sampler.begin("close_database");
    sqlite3_close(__database);
    __sampler.end();
    __sampler.end();
}

SnapshotCrawlerCache::~SnapshotCrawlerCache()
{
    __sampler.end();
    __sampler.summary();
}
