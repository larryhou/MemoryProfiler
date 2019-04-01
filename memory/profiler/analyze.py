import functools
import io
import math
import struct

from .core import PackedMemorySnapshot
from .crawler import MemorySnapshotCrawler
from .perf import TimeSampler
from .cache import CacheStorage


class SnapshotAnalyzer(object):
    def __init__(self):
        self.crawler: MemorySnapshotCrawler = None
        self.snapshot: PackedMemorySnapshot = None
        self.sampler:TimeSampler = None
        self.args = []

    @staticmethod
    def get_index_formatter(count: int):
        assert count > 0
        digit_count = int(math.ceil(math.log(count, 10)))
        return '[{:%dd}/%d]' % (digit_count, count)

    def setup(self, crawler: MemorySnapshotCrawler, sampler:TimeSampler, *args):
        self.sampler = sampler
        self.crawler = crawler
        self.snapshot = crawler.snapshot
        self.args = list(args)

    def analyze(self):
        pass


class ReferenceAnalyzer(SnapshotAnalyzer):
    def __init__(self):
        super().__init__()

    def analyze(self):
        self.sampler.begin('ReferenceAnalyzer')
        managed_objects = self.crawler.managed_objects
        index_formatter = self.get_index_formatter(len(managed_objects))
        for n in range(len(managed_objects)):
            mo = managed_objects[n]
            if mo.is_value_type: continue
            object_type = self.snapshot.typeDescriptions[mo.type_index]
            print('{} 0x{:08x} object_type={} object_index={} handle_index={}'.format(index_formatter.format(n + 1),
                                                                                      mo.address,
                                                                                      object_type.name,
                                                                                      mo.managed_object_index,
                                                                                      mo.handle_index))
            print(self.crawler.dump_managed_object_reference_chain(object_index=mo.managed_object_index, indent=2))
        self.sampler.end()


class TypeMemoryAnalyzer(SnapshotAnalyzer):
    def __init__(self):
        super().__init__()

    def analyze(self):
        self.sampler.begin('TypeMemoryAnalyzer')
        self.sampler.begin('managed_memory')
        snapshot = self.crawler.snapshot
        managed_type_set = snapshot.typeDescriptions
        managed_objects = self.crawler.managed_objects
        type_map = {}  # type: dict[int, list[int]]
        type_index_set = []  # type: list[int]
        total_native_count = 0
        total_manage_count = 0
        total_manage_memory = 0
        total_native_memory = 0
        for mo in self.crawler.managed_objects:
            managed_type = managed_type_set[mo.type_index]
            if managed_type.isValueType: continue
            managed_type.instanceCount += 1
            managed_type.managedMemory += mo.size
            total_manage_memory += mo.size
            total_manage_count += 1
            if mo.native_object_index >= 0:
                no = snapshot.nativeObjects[mo.native_object_index]
                managed_type.nativeMemory += no.size
                total_native_memory += no.size
                total_native_count += 1
            if mo.type_index not in type_map:
                type_map[mo.type_index] = []
                type_index_set.append(mo.type_index)
            type_map[mo.type_index].append(mo.managed_object_index)

        def sort_managed_object(a: int, b: int) -> int:
            obj_a = managed_objects[a]
            obj_b = managed_objects[b]
            return -1 if obj_a.size + obj_a.native_size > obj_b.size + obj_b.native_size else 1

        def sort_managed_type(a: int, b: int) -> int:
            type_a = managed_type_set[a]
            type_b = managed_type_set[b]
            return -1 if type_a.nativeMemory + type_a.managedMemory > type_b.nativeMemory + type_b.managedMemory else 1

        type_index_set.sort(key=functools.cmp_to_key(sort_managed_type))
        # memory decending
        instance_count_set = []
        for type_index, object_indice in type_map.items():
            instance_count_set.append(type_index)
            object_indice.sort(key=functools.cmp_to_key(sort_managed_object))
        # caculate instance count rank
        instance_count_set.sort(key=functools.cmp_to_key(
            lambda a, b: -1 if managed_type_set[a].instanceCount > managed_type_set[b].instanceCount else 1
        ))
        count_rank = {}
        for n in range(len(instance_count_set)): count_rank[instance_count_set[n]] = n + 1
        # print memory infomation
        print(
            '[ManagedMemory] total_memaged_memory={:,} total_managed_count={:,} total_native_memory={:,} total_native_count={:,} '.format(
                total_manage_memory, total_manage_count, total_native_memory, total_native_count
            ))
        # memory decending
        rawbytes = io.BytesIO()
        managed_rows = []
        type_number_formatter = self.get_index_formatter(len(type_index_set))
        for n in range(len(type_index_set)):
            type_index = type_index_set[n]
            managed_type = managed_type_set[type_index]
            print(
                '[Managed]{} name={!r} type_index={} managed_memory={:,} native_memory={:,} instance_count={:,} count_rank={} '.format(
                    type_number_formatter.format(n + 1), managed_type.name, managed_type.typeIndex,
                    managed_type.managedMemory, managed_type.nativeMemory, managed_type.instanceCount,
                    count_rank[type_index]
                ))
            type_instances = type_map.get(type_index)
            assert type_instances
            buffer = io.StringIO()
            buffer.write(' ' * 4)
            rawbytes.seek(0)
            for object_index in type_instances:
                no = managed_objects[object_index]
                buffer.write('{{0x{:08x}:{}|{}}},'.format(no.address, no.size, no.native_size))
                rawbytes.write(struct.pack('>Q', no.address))
            buffer.seek(buffer.tell() - 1)
            buffer.write('\n')
            buffer.seek(0)
            print(buffer.read())
            position = rawbytes.tell()
            rawbytes.seek(0)
            managed_rows.append((
                type_index, managed_type.name, managed_type.instanceCount, managed_type.managedMemory, managed_type.nativeMemory, managed_type.nativeTypeArrayIndex, rawbytes.read(position)
            ))
        self.sampler.end()
        cache = CacheStorage(uuid='{}_memory'.format(snapshot.uuid), create_mode=True)
        cache.create_table('managed', column_schemas=(
            'type_index INTEGER PRIMARY KEY',
            'name TEXT NOT NULL',
            'instance_count INTEGER',
            'managed_memory INTEGER',
            'native_memory INTEGER',
            'native_type_index INTEGER',
            'instances BLOB'
        ))
        cache.insert_table(name='managed', records=managed_rows)

        ######
        # native memory
        self.sampler.begin('native_memory')
        total_native_count = 0
        total_native_memory = 0
        type_map = {}  # type: dict[int, list[int]]
        type_index_set = []
        native_type_set = snapshot.nativeTypes
        for no in snapshot.nativeObjects:
            native_type = native_type_set[no.nativeTypeArrayIndex]
            native_type.instanceCount += 1
            native_type.nativeMemory += no.size
            total_native_count += 1
            total_native_memory += no.size
            if native_type.typeIndex not in type_map:
                type_map[native_type.typeIndex] = []
                type_index_set.append(native_type.typeIndex)
            type_map[native_type.typeIndex].append(no.nativeObjectArrayIndex)
        type_index_set.sort(key=functools.cmp_to_key(
            lambda a, b: -1 if native_type_set[a].nativeMemory > native_type_set[b].nativeMemory else 1
        ))
        native_objects = snapshot.nativeObjects

        def sort_native_object(a: int, b: int) -> int:
            obj_a = native_objects[a]
            obj_b = native_objects[b]
            if obj_a.size != obj_b.size:
                return -1 if obj_a.size > obj_b.size else 1
            return 1 if obj_a.name > obj_b.name else -1

        def sort_native_type(a: int, b: int) -> int:
            type_a = native_type_set[a]
            type_b = native_type_set[b]
            return -1 if type_a.nativeMemory > type_b.nativeMemory else 1

        type_index_set.sort(key=functools.cmp_to_key(sort_native_type))

        # memory decending
        instance_count_set = []
        for type_index, object_indice in type_map.items():
            instance_count_set.append(type_index)
            object_indice.sort(key=functools.cmp_to_key(sort_native_object))
        # caculate instance count rank
        instance_count_set.sort(key=functools.cmp_to_key(
            lambda a, b: -1 if native_type_set[a].instanceCount > native_type_set[b].instanceCount else 1
        ))
        count_rank = {}
        for n in range(len(instance_count_set)): count_rank[instance_count_set[n]] = n + 1

        # print memory infomation
        print('[NativeMemory] total_memory={:,} instance_count={:,}'.format(total_native_memory, total_native_count))

        # memory decending
        native_rows = []
        type_number_formatter = self.get_index_formatter(len(type_index_set))
        for n in range(len(type_index_set)):
            type_index = type_index_set[n]
            native_type = native_type_set[type_index]
            print('[Native]{} name={!r} type_index={} native_memory={:,} instance_count={:,} count_rank={} '.format(
                type_number_formatter.format(n + 1), native_type.name, native_type.typeIndex, native_type.nativeMemory,
                native_type.instanceCount, count_rank[type_index]
            ))
            type_instances = type_map.get(type_index)
            assert type_instances
            buffer = io.StringIO()
            buffer.write(' ' * 4)
            rawbytes.seek(0)
            for object_index in type_instances:
                no = native_objects[object_index]
                buffer.write('{{0x{:08x}:{}|{!r}}},'.format(no.nativeObjectAddress, no.size, no.name))
                rawbytes.write(struct.pack('>Q', no.nativeObjectAddress))
            buffer.seek(buffer.tell() - 1)
            buffer.write('\n')
            buffer.seek(0)
            print(buffer.read())
            position = rawbytes.tell()
            rawbytes.seek(0)
            native_rows.append((
                type_index, native_type.name, native_type.instanceCount, native_type.nativeMemory, rawbytes.read(position)
            ))
        cache.create_table('native', column_schemas=(
            'type_index INTEGER PRIMARY KEY',
            'name TEXT NOT NULL',
            'instance_count INTEGER',
            'native_memory INTEGER',
            'instances BLOB'
        ))
        cache.insert_table('native', records=native_rows)
        self.sampler.end()
        cache.commit(close_sqlite=True)
        self.sampler.end()


class StringAnalyzer(SnapshotAnalyzer):
    def __init__(self):
        super().__init__()

    def analyze(self):
        self.sampler.begin('StringAnalyzer')
        managed_strings = []
        string_type_index = self.crawler.snapshot.managedTypeIndex.system_String
        vm = self.crawler.snapshot.virtualMachineInformation
        total_size = 0
        for mo in self.crawler.managed_objects:
            if mo.type_index == string_type_index:
                managed_strings.append(mo)
                total_size += mo.size
        print('[String][Summary] instance_count={:,} total_memory={:,}'.format(len(managed_strings), total_size))
        import operator
        managed_strings.sort(key=operator.attrgetter('size'))
        index_formatter = self.get_index_formatter(len(managed_strings))
        for n in range(len(managed_strings)):
            mo = managed_strings[n]
            data = self.crawler.heap_memory.read_string(address=mo.address + vm.objectHeaderSize)
            print('[String]{} 0x{:08x}={:,} {!r}'.format(index_formatter.format(n + 1), mo.address, mo.size, data))
        self.sampler.end()


class StaticAnalyzer(SnapshotAnalyzer):
    def __init__(self):
        super().__init__()

    def analyze(self):
        pass


class ScriptAnalyzer(SnapshotAnalyzer):
    def __init__(self):
        super().__init__()

    def analyze(self):
        self.sampler.begin('ScriptAnalyzer')
        type_stats = {} # type: dict[int, list[int]]
        total_manage_memory = 0
        total_native_memory = 0
        instance_count = 0
        for mo in self.crawler.managed_objects:
            if mo.is_value_type: continue
            if mo.type_index not in type_stats:
                type_stats[mo.type_index] = [0]*3
            stats = type_stats[mo.type_index]
            stats[0] += 1
            stats[1] += mo.size
            stats[2] += mo.native_size
            total_manage_memory += mo.size
            total_native_memory += mo.native_size
            instance_count += 1
        type_indice_by_count = list(type_stats.keys())
        type_indice_by_memory = list(type_stats.keys())
        type_indice_by_count.sort(key=functools.cmp_to_key(
            lambda a, b: -1 if type_stats[a][0] > type_stats[b][0] else 1
        ))
        type_indice_by_memory.sort(key=functools.cmp_to_key(
            lambda a, b: -1 if type_stats[a][1] > type_stats[b][1] else 1
        ))
        memory_rank = {}
        for n in range(len(type_indice_by_memory)):
            type_index = type_indice_by_memory[n]
            memory_rank[type_index] = n + 1
        buffer = io.StringIO()
        buffer.write('[Script][Summary] instance_count={:,} total_managed_memory={:,} total_native_memory={:,}\n'.format(
            instance_count, total_manage_memory, total_native_memory
        ))
        index_formatter = self.get_index_formatter(len(type_indice_by_count))
        for n in range(len(type_indice_by_count)):
            type_index = type_indice_by_count[n]
            object_type = self.snapshot.typeDescriptions[type_index]
            stats = type_stats[type_index]
            buffer.write('[Script]{} name={!r} instance_count={:,} memory_rank={} managed_mameory={:,} native_memory={:,}\n'.format(
                index_formatter.format(n+1), object_type.name, stats[0], memory_rank[type_index], *stats[1:]
            ))
        buffer.seek(0)
        print(buffer.read())
        self.sampler.end()

class DelegateAnalyzer(SnapshotAnalyzer):
    def __init__(self):
        super().__init__()

    def analyze(self):
        pass
