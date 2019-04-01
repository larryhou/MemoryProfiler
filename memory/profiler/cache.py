import os
import os.path as p
import sqlite3
import struct

from .crawler import *
from .perf import TimeSampler


class CacheStorage(object):
    def __init__(self, uuid: str, create_mode: bool):
        assert uuid
        self.__workspace = p.abspath('__cache')
        self.__database_filepath = '{}/{}.db'.format(self.__workspace, uuid)
        if create_mode and p.exists(self.__database_filepath):
            os.remove(self.__database_filepath)
        self.__brand_new = not p.exists(self.__database_filepath)
        if not p.exists(self.__workspace):
            os.makedirs(self.__workspace)
        self.__connection = sqlite3.connect(database=self.__database_filepath)
        self.__connection.execute('pragma foreign_keys=ON')
        self.__cursor = self.__connection.cursor()

    @property
    def brand_new(self) -> bool:
        return self.__brand_new

    def create_table(self, name: str, column_schemas: Tuple[str], constrains: Tuple[str] = ()):
        if constrains:
            command = '''
                        CREATE TABLE {} ({},{})
                        '''.format(name, ','.join(column_schemas), ' '.join(constrains))
        else:
            command = '''
                        CREATE TABLE {} ({})
                        '''.format(name, ','.join(column_schemas))
        result = self.__cursor.execute('SELECT name FROM sqlite_master WHERE type=\'table\' AND name=?', (name,))
        if not result.fetchone(): self.__cursor.execute(command)

    def search_table(self, name: str, id: int) -> List[Tuple]:
        command = '''
                SELECT * FROM {} WHERE id=?
                '''.format(name)
        return self.__cursor.execute(command, (id,)).fetchall()

    def execute(self, command: str) -> sqlite3.Cursor:
        return self.__cursor.execute(command)

    def insert_table(self, name: str, records: List[Tuple]):
        if not records: return
        command = '''
                INSERT INTO {} VALUES ({})
                '''.format(name, ','.join(['?'] * len(records[0])))
        self.__cursor.executemany(command, records)

    def commit(self, close_sqlite: bool = False):
        self.__connection.commit()
        if close_sqlite: self.__connection.close()

class table_names(object):
    bridges = 'bridges'
    objects = 'objects'
    joints = 'joints'
    fields = 'fields'
    types = 'types'

class CrawlerCache(object):
    def __init__(self, sampler:TimeSampler):
        self.sampler:TimeSampler = sampler
        self.storage: CacheStorage = None
        self.uuid: str = ''

    def __init_database_creation(self, uuid: str):
        self.uuid = uuid
        storage = CacheStorage(uuid=uuid, create_mode=True)
        storage.create_table(name=table_names.types, column_schemas=(
            'arrayRank INTEGER',
            'assembly TEXT NOT NULL',
            'baseOrElementTypeIndex INTEGER',
            'isArray INTEGER',
            'isValueType INTEGER',
            'name TEXT NOT NULL',
            'size INTEGER',
            'staticFieldBytes BLOB',
            'typeIndex INTEGER PRIMARY KEY',
            'typeInfoAddress INTEGER',
            'nativeTypeArrayIndex INTEGER',
            'fields BLOB',
        ))
        storage.create_table(name=table_names.fields, column_schemas=(
            'id INTEGER PRIMARY KEY',
            'isStatic INTEGER',
            'name TEXT NOT NULL',
            'offset INTEGER',
            'typeIndex INTEGER REFERENCES types (typeIndex)',
        ))
        storage.create_table(name=table_names.joints, column_schemas=(
            'id INTEGER NOT NULL PRIMARY KEY',
            'object_type_index INTEGER',
            'object_index INTEGER',
            'object_address INTEGER',
            'field_type_index INTEGER',
            'field_index INTEGER',
            'field_offset INTEGER',
            'field_address INTEGER',
            'array_index INTEGER',
            'handle_index INTEGER',
            'is_static INTEGER'
        ))
        storage.create_table(name=table_names.bridges, column_schemas=(
            'id INTEGER NOT NULL PRIMARY KEY',
            'src INTEGER',
            'src_kind INTEGER',
            'dst INTEGER',
            'dst_kind INTEGER',
            'joint_id INTEGER REFERENCES joints (id)',
        ))
        storage.create_table(name=table_names.objects, column_schemas=(
            'address INTEGER NOT NULL',
            'type_index INTEGER REFERENCES types (typeIndex)',
            'managed_object_index INTEGER NOT NULL PRIMARY KEY',
            'native_object_index INTEGER',
            'handle_index INTEGER',
            'is_value_type INTEGER',
            'size INTEGER',
            'native_size INTEGER',
            'joint_id INTEGER REFERENCES joints (id)',
        ))
        self.storage = storage

    def save(self, crawler: MemorySnapshotCrawler):
        self.sampler.begin('cache_save')
        self.__init_database_creation(uuid=crawler.snapshot.uuid)
        joint_rows = []
        bridge_rows = []
        object_rows = []
        joint_count = 0
        for bridge in crawler.joint_bridges:
            if bridge.dst_kind == BridgeKind.native or bridge.src_kind == BridgeKind.native: continue
            joint = bridge.joint
            assert joint, bridge
            joint_rows.append((
                joint.id, joint.object_type_index, joint.object_index, joint.object_address, joint.field_type_index,
                joint.field_index, joint.field_offset, joint.field_address, joint.array_index, joint.handle_index,
                joint.is_static
            ))
            bridge_rows.append((
                joint_count, bridge.src, bridge.src_kind.value, bridge.dst, bridge.dst_kind.value, joint.id
            ))
            joint_count += 1
        for mo in crawler.managed_objects:
            object_rows.append((
                mo.address, mo.type_index, mo.managed_object_index, mo.native_object_index, mo.handle_index,
                mo.is_value_type, mo.size, mo.native_size, mo.joint.id
            ))

        type_rows = []
        field_rows = []
        buffer = io.BytesIO()
        for mt in crawler.snapshot.typeDescriptions:
            buffer.seek(0)
            for n in range(len(mt.fields)):
                field = mt.fields[n]
                field_id = mt.typeIndex << 16 | n
                buffer.write(struct.pack('>Q', field_id))
                field_rows.append((field_id, field.isStatic, field.name, field.offset, field.typeIndex))
            length = buffer.tell()
            buffer.seek(0)
            type_rows.append((
                mt.arrayRank, mt.assembly, mt.baseOrElementTypeIndex, mt.isArray, mt.isValueType, mt.name, mt.size,
                mt.staticFieldBytes, mt.typeIndex, mt.typeInfoAddress, mt.nativeTypeArrayIndex, buffer.read(length)
            ))

        assert bridge_rows and joint_rows
        self.sampler.begin(table_names.types)
        self.storage.insert_table(name=table_names.types, records=type_rows)
        self.sampler.end()
        self.sampler.begin(table_names.fields)
        self.storage.insert_table(name=table_names.fields, records=field_rows)
        self.sampler.end()
        self.sampler.begin(table_names.joints)
        self.storage.insert_table(name=table_names.joints, records=joint_rows)
        self.sampler.end()
        self.sampler.begin(table_names.bridges)
        self.storage.insert_table(name=table_names.bridges, records=bridge_rows)
        self.sampler.end()
        self.sampler.begin(table_names.objects)
        self.storage.insert_table(name=table_names.objects, records=object_rows)
        self.sampler.end()
        self.sampler.begin('commit')
        self.storage.commit(close_sqlite=True)
        self.sampler.end()
        self.sampler.end()

    def fill(self, crawler: MemorySnapshotCrawler) -> bool:
        self.sampler.begin('cache_load')
        storage = CacheStorage(uuid=crawler.snapshot.uuid, create_mode=False)
        if storage.brand_new: return False
        joint_map = {}
        self.sampler.begin(table_names.joints)
        for item in storage.execute('SELECT * FROM {} ORDER BY id ASC'.format(table_names.joints)).fetchall():
            joint = ActiveJoint(*item[1:])
            joint.id = item[0]
            joint_map[joint.id] = joint
        self.sampler.end()
        crawler.managed_objects = []
        self.sampler.begin(table_names.objects)
        for item in storage.execute('SELECT * FROM {} ORDER BY managed_object_index ASC'.format(table_names.objects)).fetchall():
            mo = UnityManagedObject()
            mo.address, \
            mo.type_index, \
            mo.managed_object_index, \
            mo.native_object_index, \
            mo.handle_index, \
            mo.is_value_type, \
            mo.size, \
            mo.native_size = item[:-1]
            mo.is_value_type = mo.is_value_type != 0
            mo.joint = joint_map[item[-1]]
            crawler.managed_objects.append(mo)
        self.sampler.end()
        self.sampler.begin(table_names.bridges)
        for item in storage.execute('SELECT * FROM {} ORDER BY id ASC'.format(table_names.bridges)).fetchall():
            params = list(item[1:])
            params[-1] = joint_map[params[-1]]
            bridge = JointBridge(*params)
            bridge.src_kind = BridgeKind(bridge.src_kind)
            bridge.dst_kind = BridgeKind(bridge.dst_kind)
            crawler.try_accept_connection(connection=bridge)
        self.sampler.end()
        self.sampler.end()
        return True
