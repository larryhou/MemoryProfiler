from .crawler import MemorySnapshotCrawler, UnityManagedObject, JointConnection
from .core import PackedMemorySnapshot
from typing import List
import math

class AnalyzePlugin(object):
    def __init__(self):
        self.crawler:MemorySnapshotCrawler = None
        self.snapshot:PackedMemorySnapshot = None
        self.args = []

    def setup(self, crawler:MemorySnapshotCrawler, *args):
        self.crawler = crawler
        self.snapshot = crawler.snapshot
        self.args = list(args)

    def analyze(self):
        pass

class ReferenceAnalyzer(AnalyzePlugin):
    def __init__(self):
        super().__init__()

    def analyze(self):
        import math
        managed_objects = self.crawler.managed_objects
        digit_count = math.ceil(math.log(len(managed_objects), 10))
        index_format = '[{:%dd}/%d]'%(int(digit_count), len(managed_objects))
        for n in range(len(managed_objects)):
            mo = managed_objects[n]
            if mo.is_value_type: continue
            object_type = self.snapshot.typeDescriptions[mo.type_index]
            print('{} 0x{:08x} object_type={} handle_index={}'.format(index_format.format(n+1) ,mo.address, object_type.name, mo.handle_index))
            print(self.crawler.dump_managed_object_reference_chain(object_index=mo.managed_object_index, indent=2))


class TypeMemoryAnalyzer(AnalyzePlugin):
    def __init__(self):
        super().__init__()

    @staticmethod
    def get_number_formatter(count:int):
        assert count > 0
        digit_count = int(math.ceil(math.log(count, 10)))
        return '[{:%dd}/%d]' % (digit_count, count)

    def analyze(self):
        snapshot = self.crawler.snapshot
        managed_type_set = snapshot.typeDescriptions
        managed_objects = self.crawler.managed_objects
        type_map = {} # type: dict[int, list[int]]
        type_index_set = [] # type: list[int]
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
        import functools
        def sort_managed_object(a:int, b:int)->int:
            obj_a = managed_objects[a]
            obj_b = managed_objects[b]
            return -1 if obj_a.size + obj_a.native_size > obj_b.size + obj_b.native_size else 1
        def sort_managed_type(a:int, b:int)->int:
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
        for n in range(len(instance_count_set)): count_rank[instance_count_set[n]] = n
        # print memory infomation
        print('[ManagedMemory] total_memaged_memory={:,} total_managed_count={:,} total_native_memory={:,} total_native_count={:,} '.format(
            total_manage_memory, total_manage_count, total_native_memory, total_native_count
        ))
        # memory decending
        type_number_formatter = self.get_number_formatter(len(type_index_set))
        for n in range(len(type_index_set)):
            type_index = type_index_set[n]
            managed_type = managed_type_set[type_index]
            print('[Managed]{} name={!r} type_index={} managed_memory={:,} native_memory={:,} instance_count={:,} count_rank={} '.format(
                type_number_formatter.format(n+1), managed_type.name, managed_type.typeIndex ,managed_type.managedMemory, managed_type.nativeMemory, managed_type.instanceCount, count_rank[type_index]
            ))
            type_instances = type_map.get(type_index)
            assert type_instances
            for object_index in type_instances:
                mo = managed_objects[object_index]
                print('    address={:08x} type_index={} size={:,} native_size={:,} '.format(mo.address, mo.type_index, mo.size, mo.native_size))


        native_type_set = snapshot.nativeTypes
        for no in snapshot.nativeObjects:
            native_type = native_type_set[no.nativeTypeArrayIndex]
            native_type.instanceCount += 1
            native_type.nativeMemory += no.size


class StringAnalyzer(AnalyzePlugin):
    def __init__(self):
        super().__init__()

    def analyze(self):
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
        import math
        digit_format = '[{:%dd}/%d]' % (int(math.ceil(math.log(len(managed_strings), 10))), len(managed_strings))
        for n in range(len(managed_strings)):
            mo = managed_strings[n]
            data = self.crawler.heap_memory.read_string(address=mo.address + vm.objectHeaderSize)
            print('[String]{} 0x{:08x}={:,} {!r}'.format(digit_format.format(n+1), mo.address, mo.size, data))

class StaticAnalyzer(AnalyzePlugin):
    def __init__(self):
        super().__init__()

    def analyze(self):
        pass

class ScriptAnalyzer(AnalyzePlugin):
    def __init__(self):
        super().__init__()

    def analyze(self):
        pass

class DelegateAnalyzer(AnalyzePlugin):
    def __init__(self):
        super().__init__()

    def analyze(self):
        pass