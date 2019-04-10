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
    auto rc = sqlite3_open(filepath, &__database);
    assert(rc == 0);
    
    sqlite3_exec(__database, "PRAGMA FOREIGN_KEYS=ON;", nullptr, nullptr, nullptr);
}

void CrawlerCache::create(const char *sql)
{
    char *errmsg;
    sqlite3_exec(__database, sql, nullptr, nullptr, &errmsg);
}

void CrawlerCache::createNativeTypeTable()
{
    create("CREATE TABLE nativeTypes (" \
           "typeIndex INTEGER PRIMARY KEY," \
           "name TEXT NOT NULL," \
           "nativeBaseTypeArrayIndex INTEGER," \
           "managedTypeArrayIndex INTEGER);");
}

void CrawlerCache::insert(Array<PackedNativeType> &nativeTypes)
{
    insert<PackedNativeType>("INSERT INTO nativeTypes VALUES (?1, ?2, ?3, ?4)",
                             nativeTypes, [](PackedNativeType &nt, sqlite3_stmt *stmt)
                             {
                                 sqlite3_bind_int(stmt, 1, nt.typeIndex);
                                 sqlite3_bind_text(stmt, 2, nt.name->c_str(), (int)nt.name->size(), SQLITE_STATIC);
                                 sqlite3_bind_int64(stmt, 3, nt.nativeBaseTypeArrayIndex);
                                 sqlite3_bind_int(stmt, 4, nt.managedTypeArrayIndex);
                             });
}

void CrawlerCache::createNativeObjectTable()
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

void CrawlerCache::insert(Array<PackedNativeUnityEngineObject> &nativeObjects)
{
    insert<PackedNativeUnityEngineObject>("INSERT INTO nativeObjects VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, 9?, 10?, 11?)",
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

void CrawlerCache::createTypeTable()
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

void CrawlerCache::createFieldTable()
{
    create("CREATE TABLE fields (" \
           "id INTEGER PRIMARY KEY," \
           "isStatic INTEGER," \
           "name TEXT NOT NULL," \
           "offset INTEGER," \
           "typeIndex INTEGER REFERENCES types (typeIndex));");
}

void CrawlerCache::insert(Array<TypeDescription> &types)
{
    insert<TypeDescription>("INSERT INTO types VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12)",
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
                                sqlite3_bind_blob(stmt, 8, t.staticFieldBytes->items, t.staticFieldBytes->size, SQLITE_STATIC);
                                sqlite3_bind_int(stmt, 9, t.typeIndex);
                                sqlite3_bind_int64(stmt, 10, t.typeInfoAddress);
                                sqlite3_bind_int64(stmt, 11, t.nativeTypeArrayIndex);
                                sqlite3_bind_blob(stmt, 12, __buffer, (int)(iter - __buffer), SQLITE_STATIC);
                            });
    
    // type fields
    insert<TypeDescription>("INSERT INTO fields VALUES (?1, ?2, ?3, ?4, ?5)",
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

void CrawlerCache::createJointTable()
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

void CrawlerCache::insert(InstanceManager<EntityJoint> &joints)
{
    insert<EntityJoint>("INSERT INTO joints VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)",
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

void CrawlerCache::createConnectionTable()
{
    create("CREATE TABLE connections (" \
           "id INTEGER PRIMARY KEY," \
           "from INTEGER," \
           "fromKind INTEGER," \
           "to INTEGER," \
           "toKind INTEGER," \
           "jointArrayIndex INTEGER REFERENCES joints (jointArrayIndex));");
}

void CrawlerCache::insert(InstanceManager<EntityConnection> &connections)
{
    insert<EntityConnection>("INSERT INTO connections VALUES (?1, ?2, ?3, ?4, ?5, ?6)",
                             connections, [](EntityConnection &ec, sqlite3_stmt *stmt)
                             {
                                 sqlite3_bind_int(stmt, 1, ec.connectionArrayIndex);
                                 sqlite3_bind_int(stmt, 2, ec.from);
                                 sqlite3_bind_int(stmt, 3, ec.fromKind);
                                 sqlite3_bind_int(stmt, 4, ec.to);
                                 sqlite3_bind_int(stmt, 5, ec.toKind);
                                 sqlite3_bind_int(stmt, 6, ec.jointArrayIndex);
                             });
}

void CrawlerCache::createObjectTable()
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

void CrawlerCache::insert(InstanceManager<ManagedObject> &objects)
{
    insert<ManagedObject>("INSERT INTO objects VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)",
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

//MemorySnapshotCrawler &CrawlerCache::read()
//{
//
//}

void CrawlerCache::save(MemorySnapshotCrawler &crawler)
{
    char filepath[64];
    sprintf(filepath, "__cpp_cache/%s.db", crawler.snapshot.uuid->c_str());
    open(filepath);
    
    createNativeTypeTable();
    createNativeObjectTable();
    createTypeTable();
    createFieldTable();
    createJointTable();
    createObjectTable();
    createConnectionTable();
    
    insert(*crawler.snapshot.nativeTypes);
    insert(*crawler.snapshot.nativeObjects);
    insert(*crawler.snapshot.typeDescriptions);
    insert(crawler.joints);
    insert(crawler.managedObjects);
    insert(crawler.connections);
    
    sqlite3_close(__database);
}

CrawlerCache::~CrawlerCache()
{
    
}
