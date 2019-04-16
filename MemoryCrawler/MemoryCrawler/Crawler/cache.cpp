//
//  cache.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/9.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "cache.h"
#include <sys/stat.h>
#include <string>

SnapshotCrawlerCache::SnapshotCrawlerCache()
{
    __sampler.begin("SnapshotCrawlerCache");
}

void SnapshotCrawlerCache::open(const char *filepath)
{
    auto rc = sqlite3_open(filepath, &__database);
    assert(rc == 0);
    
    char *errmsg;
    sqlite3_exec(__database, "PRAGMA FOREIGN_KEYS=OFF;", nullptr, nullptr, &errmsg);
//    sqlite3_exec(__database, "PRAGMA journal_mode = MEMORY;", nullptr, nullptr, &errmsg);
}

void SnapshotCrawlerCache::create(const char *sql)
{
    char *errmsg;
    sqlite3_exec(__database, sql, nullptr, nullptr, &errmsg);
}

static int sqliteCallbackSelectCount(void *context, int argc, char **argv, char **columns)
{
    auto ptr = (int *)context;
    *ptr = atoi(argv[0]);
    return 0;
}

int32_t SnapshotCrawlerCache::selectCount(const char *name)
{
    char *errmsg;
    
    int count = 0;
    sprintf(__buffer, "select count(*) from %s;", name);
    sqlite3_exec(__database, __buffer, sqliteCallbackSelectCount, &count, &errmsg);
    return count;
}

void SnapshotCrawlerCache::createNativeTypeTable()
{
    create("CREATE TABLE nativeTypes (" \
           "typeIndex INTEGER PRIMARY KEY," \
           "name TEXT NOT NULL," \
           "nativeBaseTypeArrayIndex INTEGER," \
           "managedTypeArrayIndex INTEGER,"\
           "instanceCount INTEGER," \
           "instanceMemory INTEGER);");
}

void SnapshotCrawlerCache::insert(Array<PackedNativeType> &nativeTypes)
{
    insert<PackedNativeType>("INSERT INTO nativeTypes VALUES (?1, ?2, ?3, ?4, ?5, ?6);",
                             nativeTypes, [](PackedNativeType &nt, sqlite3_stmt *stmt)
                             {
                                 sqlite3_bind_int(stmt, 1, nt.typeIndex);
                                 sqlite3_bind_text(stmt, 2, nt.name->c_str(), (int)nt.name->size(), SQLITE_STATIC);
                                 sqlite3_bind_int64(stmt, 3, nt.nativeBaseTypeArrayIndex);
                                 sqlite3_bind_int(stmt, 4, nt.managedTypeArrayIndex);
                                 sqlite3_bind_int(stmt, 5, nt.instanceCount);
                                 sqlite3_bind_int(stmt, 6, nt.instanceMemory);
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
           "fields INTEGER," \
           "instanceCount INTEGER," \
           "instanceMemory INTEGER," \
           "nativeMemory INTEGER);");
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
    insert<TypeDescription>("INSERT INTO types VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15);",
                            types, [this](TypeDescription &t, sqlite3_stmt *stmt)
                            {
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
                                sqlite3_bind_int(stmt, 11, t.nativeTypeArrayIndex);
                                sqlite3_bind_int(stmt, 12, t.fields->size);
                                sqlite3_bind_int(stmt, 13, t.instanceCount);
                                sqlite3_bind_int(stmt, 14, t.instanceMemory);
                                sqlite3_bind_int(stmt, 15, t.nativeMemory);
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

void SnapshotCrawlerCache::removeRedundants(MemorySnapshotCrawler &crawler)
{
    int32_t jointArrayIndex = 0;
    auto &joints = crawler.joints;
    for (auto i = 0; i < joints.size(); i++)
    {
        auto &ej = joints[i];
        if (ej.isConnected)
        {
            ej.jointArrayIndex = jointArrayIndex++;
        }
    }
    
    auto &managedObjects = crawler.managedObjects;
    for (auto i = 0; i < managedObjects.size(); i++)
    {
        auto &mo = managedObjects[i];
        auto &ej = joints[mo.jointArrayIndex];
        mo.jointArrayIndex = ej.jointArrayIndex;
    }
}

void SnapshotCrawlerCache::insert(InstanceManager<EntityJoint> &joints)
{
    insert<EntityJoint>("INSERT INTO joints VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11);",
                        joints, [](EntityJoint &ej, sqlite3_stmt *stmt)
                        {
                            if (!ej.isConnected){return;}
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
           "toKind INTEGER);");
}

void SnapshotCrawlerCache::insert(InstanceManager<EntityConnection> &connections)
{
    insert<EntityConnection>("INSERT INTO connections VALUES (?1, ?2, ?3, ?4, ?5);",
                             connections, [](EntityConnection &ec, sqlite3_stmt *stmt)
                             {
                                 sqlite3_bind_int(stmt, 1, ec.connectionArrayIndex);
                                 sqlite3_bind_int(stmt, 2, ec.from);
                                 sqlite3_bind_int(stmt, 3, (int)ec.fromKind);
                                 sqlite3_bind_int(stmt, 4, ec.to);
                                 sqlite3_bind_int(stmt, 5, (int)ec.toKind);
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

void SnapshotCrawlerCache::createVMTable()
{
    create("CREATE TABLE vm (" \
           "allocationGranularity INTEGER," \
           "arrayBoundsOffsetInHeader INTEGER," \
           "arrayHeaderSize INTEGER," \
           "arraySizeOffsetInHeader INTEGER," \
           "heapFormatVersion INTEGER," \
           "objectHeaderSize INTEGER," \
           "pointerSize INTEGER);");
}

void SnapshotCrawlerCache::createStringTable()
{
    create("CREATE TABLE strings (" \
           "id INTEGER PRIMARY KEY," \
           "size INTEGER," \
           "data TEXT NOT NULL," \
           "address INTEGER);");
}

void SnapshotCrawlerCache::insertStringTable(MemorySnapshotCrawler &crawler)
{
    char *errmsg;
    sqlite3_stmt *stmt;
    
    const char sql[] = "INSERT INTO strings VALUES (?1, ?2, ?3, ?4);";
    sqlite3_exec(__database, "BEGIN TRANSACTION", nullptr, nullptr, &errmsg);
    sqlite3_prepare_v2(__database, sql, (int)strlen(sql), &stmt, nullptr);
    
    int32_t sequence = 0;
    int32_t stringTypeIndex = crawler.snapshot.managedTypeIndex.system_String;
    auto &managedObjects = crawler.managedObjects;
    for (auto i = 0; i < managedObjects.size(); i++)
    {
        auto &mo = managedObjects[i];
        if (stringTypeIndex == mo.typeIndex)
        {
            int32_t size;
            auto target = crawler.getString(mo.address, size);
            
            sqlite3_bind_int(stmt, 1, sequence++);
            sqlite3_bind_int(stmt, 2, size);
            sqlite3_bind_text16(stmt, 3, target, size, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 4, mo.address);
            
            if (sqlite3_step(stmt) != SQLITE_DONE) {}
            sqlite3_reset(stmt);
        }
    }
    
    sqlite3_exec(__database, "COMMIT TRANSACTION", nullptr, nullptr, &errmsg);
    sqlite3_finalize(stmt);
}

MemorySnapshotCrawler *SnapshotCrawlerCache::read(const char *uuid)
{
    __sampler.begin("SnapshotCrawlerCache::read");
    char filepath[64];
    sprintf(filepath, "%s/%s.db", __workspace, uuid);
    
    auto crawler = new MemorySnapshotCrawler();
    
    __sampler.begin("open");
    open(filepath);
    __sampler.end();
    
    __sampler.begin("read_PackedMemorySnapshot");
    auto &snapshot = crawler->snapshot;
    
    __sampler.begin("read_native_types");
    snapshot.nativeTypes = new Array<PackedNativeType>(selectCount("nativeTypes"));
    select<PackedNativeType>("select * from nativeTypes;", *snapshot.nativeTypes,
                             [](PackedNativeType &nt, sqlite3_stmt *stmt)
                             {
                                 nt.typeIndex = sqlite3_column_int(stmt, 0);
                                 nt.name = new string((char *)sqlite3_column_text(stmt, 1));
                                 nt.nativeBaseTypeArrayIndex = sqlite3_column_int(stmt, 2);
                                 nt.managedTypeArrayIndex = sqlite3_column_int(stmt, 3);
                                 nt.instanceCount = sqlite3_column_int(stmt, 4);
                                 nt.instanceMemory = sqlite3_column_int(stmt, 5);
                             });
    __sampler.end(); // read_native_types
    
    __sampler.begin("read_native_objects");
    snapshot.nativeObjects = new Array<PackedNativeUnityEngineObject>(selectCount("nativeObjects"));
    select<PackedNativeUnityEngineObject>("select * from nativeObjects;", *snapshot.nativeObjects,
                                          [](PackedNativeUnityEngineObject &nt, sqlite3_stmt *stmt)
                                          {
                                              nt.hideFlags = sqlite3_column_int(stmt, 0);
                                              nt.instanceId = sqlite3_column_int(stmt, 1);
                                              nt.isDontDestroyOnLoad = (bool)sqlite3_column_int(stmt, 2);
                                              nt.isManager = (bool)sqlite3_column_int(stmt, 3);
                                              nt.isPersistent = (bool)sqlite3_column_int(stmt, 4);
                                              nt.name = new string((char *)sqlite3_column_text(stmt, 5));
                                              nt.nativeObjectAddress = sqlite3_column_int(stmt, 6);
                                              nt.nativeTypeArrayIndex = sqlite3_column_int(stmt, 7);
                                              nt.size = sqlite3_column_int(stmt, 8);
                                              nt.managedObjectArrayIndex = sqlite3_column_int(stmt, 9);
                                              nt.nativeObjectArrayIndex = sqlite3_column_int(stmt, 10);
                                          });
    __sampler.end(); // read_native_objects
    
    __sampler.begin("read_managed_types");
    snapshot.typeDescriptions = new Array<TypeDescription>(selectCount("types"));
    select<TypeDescription>("select * from types;", *snapshot.typeDescriptions,
                            [](TypeDescription &mt, sqlite3_stmt *stmt)
                            {
                                mt.arrayRank = sqlite3_column_int(stmt, 0);
                                mt.assembly = new string((char *)sqlite3_column_text(stmt, 1));
                                mt.baseOrElementTypeIndex = sqlite3_column_int(stmt, 2);
                                mt.isArray = (bool)sqlite3_column_int(stmt, 3);
                                mt.isValueType = (bool)sqlite3_column_int(stmt, 4);
                                mt.name = new string((char *)sqlite3_column_text(stmt, 5));
                                mt.size = sqlite3_column_int(stmt, 6);
                                {
                                    auto size = sqlite3_column_bytes(stmt, 7);
                                    if (size > 0)
                                    {
                                        auto data = (const char *)sqlite3_column_blob(stmt, 7);
                                        mt.staticFieldBytes = new Array<byte_t>(size);
                                        memcpy(mt.staticFieldBytes->items, data, size);
                                    }
                                }
                                mt.typeIndex = sqlite3_column_int(stmt, 8);
                                mt.typeInfoAddress = sqlite3_column_int64(stmt, 9);
                                mt.nativeTypeArrayIndex = sqlite3_column_int(stmt, 10);
                                {
                                    auto size = sqlite3_column_int(stmt, 11);
                                    if (size > 0)
                                    {
                                        mt.fields = new Array<FieldDescription>(size);
                                    }
                                }
                                mt.instanceCount = sqlite3_column_int(stmt, 12);
                                mt.instanceMemory = sqlite3_column_int(stmt, 13);
                                mt.nativeMemory = sqlite3_column_int(stmt, 14);
                            });
    __sampler.begin("read_type_fields");
    int32_t typeIndex = 0;
    TypeDescription *hookType;
    select("select * from fields;", (int32_t)selectCount("fields"),
           [&](sqlite3_stmt *stmt)
           {
               auto id = sqlite3_column_int64(stmt, 0);
               auto fieldHookTypeIndex = id >> 16;
               auto fieldSlotIndex = id & 0xFFFF;
               if (hookType == nullptr || fieldHookTypeIndex != typeIndex)
               {
                   hookType = &snapshot.typeDescriptions->items[fieldHookTypeIndex];
                   typeIndex = (int32_t)fieldHookTypeIndex;
               }
               auto &field = hookType->fields->items[fieldSlotIndex];
               field.isStatic = (bool)sqlite3_column_int(stmt, 1);
               field.name = new string((char *)sqlite3_column_text(stmt, 2));
               field.offset = sqlite3_column_int(stmt, 3);
               field.typeIndex = sqlite3_column_int(stmt, 4);
           });
    __sampler.end(); // read_fields
    __sampler.end(); // read_type_fields
    __sampler.begin("read_vm");
    auto &vm = snapshot.virtualMachineInformation;
    select("select * from vm;", 1,
           [&](sqlite3_stmt *stmt)
           {
               vm.allocationGranularity = sqlite3_column_int(stmt, 0);
               vm.arrayBoundsOffsetInHeader = sqlite3_column_int(stmt, 1);
               vm.arrayHeaderSize = sqlite3_column_int(stmt, 2);
               vm.arraySizeOffsetInHeader = sqlite3_column_int(stmt, 3);
               vm.heapFormatVersion = sqlite3_column_int(stmt, 4);
               vm.objectHeaderSize = sqlite3_column_int(stmt, 5);
               vm.pointerSize = sqlite3_column_int(stmt, 6);
           });
    __sampler.end(); // read_vm
    __sampler.end(); // read_snapshot
    
    __sampler.begin("read_MemorySnapshotCrawler");
    __sampler.begin("read_joints");
    
    select<EntityJoint>("select * from joints;", selectCount("joints"), crawler->joints,
                        [](EntityJoint &ej, sqlite3_stmt *stmt)
                        {
                            ej.isConnected = true;
                            ej.jointArrayIndex = sqlite3_column_int(stmt, 0);
                            ej.hookTypeIndex = sqlite3_column_int(stmt, 1);
                            ej.hookObjectIndex = sqlite3_column_int(stmt, 2);
                            ej.hookObjectAddress = sqlite3_column_int64(stmt, 3);
                            ej.fieldTypeIndex = sqlite3_column_int(stmt, 4);
                            ej.fieldSlotIndex = sqlite3_column_int(stmt, 5);
                            ej.fieldOffset = sqlite3_column_int(stmt, 6);
                            ej.fieldAddress = sqlite3_column_int64(stmt, 7);
                            ej.elementArrayIndex = sqlite3_column_int(stmt, 8);
                            ej.gcHandleIndex = sqlite3_column_int(stmt, 9);
                            ej.isStatic = (bool)sqlite3_column_int(stmt, 10);
                        });
    __sampler.end(); // read_joints
    
    __sampler.begin("read_managed_objects");
    select<ManagedObject>("select * from objects;", selectCount("objects"), crawler->managedObjects,
                          [](ManagedObject &mo, sqlite3_stmt *stmt)
                          {
                              mo.address = sqlite3_column_int64(stmt, 0);
                              mo.typeIndex = sqlite3_column_int(stmt, 1);
                              mo.managedObjectIndex = sqlite3_column_int(stmt, 2);
                              mo.nativeObjectIndex = sqlite3_column_int(stmt, 3);
                              mo.gcHandleIndex = sqlite3_column_int(stmt, 4);
                              mo.isValueType = (bool)sqlite3_column_int(stmt, 5);
                              mo.size = sqlite3_column_int(stmt, 6);
                              mo.nativeSize = sqlite3_column_int(stmt, 7);
                              mo.jointArrayIndex = sqlite3_column_int(stmt, 8);
                          });
    __sampler.end(); // read_managed_objects
    __sampler.begin("read_connections");
    select<EntityConnection>("select * from connections;", selectCount("connections"), crawler->connections,
                             [&](EntityConnection &ec, sqlite3_stmt *stmt)
                             {
                                 ec.connectionArrayIndex = sqlite3_column_int(stmt, 0);
                                 ec.from = sqlite3_column_int(stmt, 1);
                                 ec.fromKind = (ConnectionKind)sqlite3_column_int(stmt, 2);
                                 ec.to = sqlite3_column_int(stmt, 3);
                                 ec.toKind = (ConnectionKind)sqlite3_column_int(stmt, 4);
                                 crawler->tryAcceptConnection(ec);
                             });
    __sampler.end(); // read_connections
    __sampler.end(); // read_crawler
    __sampler.end(); // ::read
    
    __sampler.end();
    __sampler.summary();
    return crawler;
}

void SnapshotCrawlerCache::select(const char *sql, int32_t size, std::function<void (sqlite3_stmt *)> kernel)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(__database, sql, (int)strlen(sql), &stmt, nullptr);
    
    for (auto i = 0; i < size; i++)
    {
        if (sqlite3_step(stmt) != SQLITE_DONE) {}
        kernel(stmt);
    }
    
    sqlite3_finalize(stmt);
}

void SnapshotCrawlerCache::insertVMTable(VirtualMachineInformation &vm)
{
    char *errmsg;
    sqlite3_stmt *stmt;
    
    const char sql[] = "INSERT INTO vm VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7);";
    sqlite3_exec(__database, "BEGIN TRANSACTION", nullptr, nullptr, &errmsg);
    sqlite3_prepare_v2(__database, sql, (int)strlen(sql), &stmt, nullptr);
    
    sqlite3_bind_int(stmt, 1, vm.allocationGranularity);
    sqlite3_bind_int(stmt, 2, vm.arrayBoundsOffsetInHeader);
    sqlite3_bind_int(stmt, 3, vm.arrayHeaderSize);
    sqlite3_bind_int(stmt, 4, vm.arraySizeOffsetInHeader);
    sqlite3_bind_int(stmt, 5, vm.heapFormatVersion);
    sqlite3_bind_int(stmt, 6, vm.objectHeaderSize);
    sqlite3_bind_int(stmt, 7, vm.pointerSize);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {}
    sqlite3_reset(stmt);
    
    sqlite3_exec(__database, "COMMIT TRANSACTION", nullptr, nullptr, &errmsg);
    sqlite3_finalize(stmt);
}

void SnapshotCrawlerCache::save(MemorySnapshotCrawler &crawler)
{
    if (crawler.snapshot.uuid == string()) {return;}
    
    __sampler.begin("SnapshotCrawlerCache::save");
    mkdir(__workspace, 0777);
    
    char filepath[64];
    sprintf(filepath, "%s/%s.db", __workspace, crawler.snapshot.uuid.c_str());
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
    
    __sampler.begin("remove_redundants");
    removeRedundants(crawler);
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
    
    __sampler.begin("insert_vm");
    createVMTable();
    insertVMTable(crawler.snapshot.virtualMachineInformation);
    __sampler.end();
    
    __sampler.begin("insert_strings");
    createStringTable();
    insertStringTable(crawler);
    __sampler.end();
    
    __sampler.end();
    
    __sampler.end();
    __sampler.summary();
}

SnapshotCrawlerCache::~SnapshotCrawlerCache()
{
    if (__database != nullptr)
    {
        sqlite3_close(__database);
    }
}
