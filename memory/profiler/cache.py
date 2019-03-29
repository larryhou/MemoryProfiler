import sqlite3
from typing import Tuple, List
import os
import os.path as p
from .crawler import *


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


class CrawlerCache(object):
    def __init__(self):
        self.storage: CacheStorage = None
        self.uuid: str = ''

    def __init_database_creation(self, uuid: str):
        self.uuid = uuid
        storage = CacheStorage(uuid=uuid, create_mode=True)
        storage.create_table(name='joints', column_schemas=(
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
        storage.create_table(name='bridges', column_schemas=(
            'id INTEGER NOT NULL PRIMARY KEY',
            'src INTEGER',
            'src_kind INTEGER',
            'dst INTEGER',
            'dst_kind INTEGER',
            'joint_id INTEGER REFERENCES joints (id)',
        ))
        storage.create_table(name='objects', column_schemas=(
            'address INTEGER NOT NULL',
            'type_index INTEGER',
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
                1 if joint.is_static else 0
            ))
            bridge_rows.append((
                joint_count, bridge.src, bridge.src_kind.value, bridge.dst, bridge.dst_kind.value, joint.id
            ))
            joint_count += 1
        for mo in crawler.managed_objects:
            object_rows.append((
                mo.address, mo.type_index, mo.managed_object_index, mo.native_object_index, mo.handle_index,
                1 if mo.is_value_type else 0, mo.size, mo.native_size, mo.joint.id
            ))
        assert bridge_rows and joint_rows
        self.storage.insert_table(name='joints', records=joint_rows)
        self.storage.insert_table(name='bridges', records=bridge_rows)
        self.storage.insert_table(name='objects', records=object_rows)
        self.storage.commit(close_sqlite=True)

    @classmethod
    def fill(cls, crawler: MemorySnapshotCrawler) -> bool:
        storage = CacheStorage(uuid=crawler.snapshot.uuid, create_mode=False)
        if storage.brand_new: return False
        joint_map = {}
        for item in storage.execute('SELECT * FROM joints ORDER BY id ASC').fetchall():
            joint = ActiveJoint(*item[1:])
            joint.id = item[0]
            joint_map[joint.id] = joint
        crawler.managed_objects = []
        for item in storage.execute('SELECT * FROM objects ORDER BY managed_object_index ASC').fetchall():
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
        for item in storage.execute('SELECT * FROM bridges ORDER BY id ASC').fetchall():
            params = list(item[1:])
            params[-1] = joint_map[params[-1]]
            bridge = JointBridge(*params)
            bridge.src_kind = BridgeKind(bridge.src_kind)
            bridge.dst_kind = BridgeKind(bridge.dst_kind)
            crawler.try_accept_connection(connection=bridge)
        return True
