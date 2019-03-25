from .core import *
from .heap import *
from typing import List, Dict, Tuple
import enum, sys


class ConnectionKind(enum.Enum):
    none, handle, native, managed, static = range(5)

class KeepAliveJoint(object):
    def __init__(self, object_type_index:int = -1, object_index:int = -1, field_type_index:int = -1, field_index:int = -1, array_index:int = -1, handle_index:int = -1, is_static:bool = False):
        # gcHandle joint
        self.handle_index:int = handle_index
        # managed object joint
        self.object_type_index: int = object_type_index
        self.object_index: int = object_index
        # reference point
        self.field_type_index:int = field_type_index
        self.field_index:int = field_index
        self.array_index:int = array_index
        self.is_static = is_static

    def clone(self, object_type_index:int = -1, object_index:int = -1, field_type_index:int = -1, field_index:int = -1, array_index:int = -1, handle_index:int = -1)-> 'KeepAliveJoint':
        rj = KeepAliveJoint()
        rj.handle_index = handle_index if handle_index >= 0 else self.handle_index
        rj.object_type_index = object_type_index if object_type_index >= 0 else self.object_type_index
        rj.object_index = object_index if object_index >= 0 else self.object_index
        rj.field_type_index = field_type_index if field_type_index >= 0 else self.field_type_index
        rj.field_index = field_index if field_index >= 0 else self.field_index
        rj.array_index = array_index if array_index >= 0 else self.array_index
        rj.is_static = self.is_static
        return rj

class JointConnection(object):
    def __init__(self, src=0, src_kind=ConnectionKind.none, dst=0, dst_kind=ConnectionKind.none, joint:KeepAliveJoint = None):
        self.src: int = src
        self.dst: int = dst
        self.src_kind: ConnectionKind = src_kind
        self.dst_kind: ConnectionKind = dst_kind
        self.joint:KeepAliveJoint = joint

class UnityManagedObject(object):
    def __init__(self):
        self.address: int = -1
        self.type_index: int = -1
        self.managed_object_index: int = -1
        self.native_object_index: int = -1
        self.handle_index: int = -1
        self.size: int = 0

    def __repr__(self):
        return '[UnityManagedObject] address={:08x} type_index={} native_object_index={} managed_object_index={} handle_index={} size={}'.format(
            self.address, self.type_index, self.native_object_index, self.managed_object_index, self.handle_index, self.size
        )

class MemorySnapshotCrawler(object):
    def __init__(self, snapshot: PackedMemorySnapshot):
        self.managed_objects: List[UnityManagedObject] = []
        self.snapshot: PackedMemorySnapshot = snapshot
        self.snapshot.initialize()
        self.vm = snapshot.virtualMachineInformation

        # record crawling footprint
        self.__visit: Dict[int, int] = {}
        self.__heap_reader: HeapReader = HeapReader(snapshot=snapshot)
        self.__cached_ptr: FieldDescription = snapshot.cached_ptr

        # type index utils
        self.__nt_index = self.snapshot.nativeTypeIndex
        self.__mt_index = self.snapshot.managedTypeIndex

        # connections
        self.managed_connections: List[JointConnection] = []
        self.mono_script_connections: List[JointConnection] = []
        self.connections_from: Dict[int, List[JointConnection]] = {}
        self.connections_to: Dict[int, List[JointConnection]] = {}

        self.__type_address_map: Dict[int, int] = {}
        self.__native_object_address_map: Dict[int, int] = {}
        self.__managed_object_address_map: Dict[int, int] = {}
        self.__managed_native_address_map: Dict[int, int] = {}
        self.__handle_address_map: Dict[int, int] = {}

    def crawl(self):
        self.init_managed_types()
        self.init_managed_connections()
        self.init_mono_script_connections()
        self.crawl_handles()
        self.crawl_static()

    def init_mono_script_connections(self):
        for n in range(len(self.managed_connections)):
            connection = self.managed_connections[n]
            if connection.dst_kind == ConnectionKind.native \
                    and self.snapshot.nativeObjects[connection.dst].nativeTypeArrayIndex == self.__nt_index.MonoScript:
                self.mono_script_connections.append(connection)

    def init_managed_types(self):
        managed_types = self.snapshot.typeDescriptions
        for n in range(len(managed_types)):
            mt = managed_types[n]
            mt.isUnityEngineObjectType = self.is_subclass_of_managed_type(mt, self.__mt_index.unityengine_Object)

    def init_managed_connections(self, exclude_native: bool = False):
        managed_start = 0
        managed_stop = managed_start + len(self.snapshot.gcHandles)
        native_start = managed_start + managed_stop
        native_stop = native_start + len(self.snapshot.nativeObjects)
        managed_connections = []
        for it in self.snapshot.connections:
            connection = JointConnection(src=it.from_, dst=it.to)
            connection.src_kind = ConnectionKind.handle
            if native_start <= connection.src < native_stop:
                connection.src -= native_start
                connection.src_kind = ConnectionKind.native
            connection.dst_kind = ConnectionKind.handle
            if native_start <= connection.dst < native_stop:
                connection.dst -= native_start
                connection.dst_kind = ConnectionKind.native
            if exclude_native and connection.src_kind == ConnectionKind.native: continue
            managed_connections.append(connection)
            self.accept_connection(connection=connection)
        self.managed_connections = managed_connections

    def accept_connection(self, connection:JointConnection):
        if connection.src_kind != ConnectionKind.none and connection.src != -1:
            key = self.get_connection_key(kind=connection.src_kind, index=connection.src)
            if key not in self.connections_from:
                self.connections_from[key] = []
            self.connections_from[key].append(connection)
        if connection.dst_kind != ConnectionKind.none and connection.dst != -1:
            if connection.dst < 0: connection.dst = -connection.dst
            key = self.get_connection_key(kind=connection.dst_kind, index=connection.dst)
            if key not in self.connections_to:
                self.connections_to[key] = []
            self.connections_to[key].append(connection)

    def get_connection_key(self, kind: ConnectionKind, index: int):
        return kind.value << 50 + index

    def find_type_of_address(self, address: int) -> int:
        type_index = self.find_type_at_type_info_address(type_info_address=address)
        if type_index != -1: return type_index
        type_ptr = self.__heap_reader.read_pointer(address)
        if type_ptr == 0: return -1
        vtable_ptr = self.__heap_reader.read_pointer(type_ptr)
        if vtable_ptr != 0:
            return self.find_type_at_type_info_address(vtable_ptr)
        return self.find_type_at_type_info_address(type_ptr)

    def find_type_at_type_info_address(self, type_info_address: int) -> int:
        if type_info_address == 0: return -1
        if not self.__type_address_map:
            managed_types = self.snapshot.typeDescriptions
            for t in managed_types:
                self.__type_address_map[t.typeInfoAddress] = t.typeIndex
        type_index = self.__type_address_map.get(type_info_address)
        return -1 if type_index is None else type_index

    def find_managed_object_of_native_object(self, native_address: int):
        if native_address == 0: return -1
        if not self.__managed_native_address_map:
            for n in range(len(self.managed_objects)):
                mo = self.managed_objects[n]
                if mo.native_object_index >= 0:
                    no = self.snapshot.nativeObjects[mo.native_object_index]
                    self.__managed_native_address_map[no.nativeObjectAddress] = n
        manage_index = self.__managed_native_address_map.get(native_address)
        return -1 if manage_index is None else manage_index

    def find_managed_object_at_address(self, address: int) -> int:
        if address == 0: return -1
        if not self.__managed_object_address_map:
            for n in range(len(self.managed_objects)):
                mo = self.managed_objects[n]
                self.__managed_object_address_map[mo.address] = n
        object_index = self.__managed_object_address_map.get(address)
        return -1 if object_index is None else object_index

    def find_native_object_at_address(self, address: int):
        if address == 0: return -1
        if not self.__native_object_address_map:
            for n in range(len(self.snapshot.nativeObjects)):
                no = self.snapshot.nativeObjects[n]
                self.__native_object_address_map[no.nativeObjectAddress] = n
        object_index = self.__native_object_address_map.get(address)
        return -1 if object_index is None else object_index

    def find_handle_with_target_address(self, address: int):
        if address == 0: return -1
        if not self.__handle_address_map:
            for n in range(len(self.snapshot.gcHandles)):
                handle = self.snapshot.gcHandles[n]
                self.__handle_address_map[handle.target] = n
        handle_index = self.__handle_address_map.get(address)
        return -1 if handle_index is None else handle_index

    def get_connections_of(self, kind: ConnectionKind, managed_object_index: int) -> List[JointConnection]:
        key = self.get_connection_key(kind=kind, index=managed_object_index)
        references = self.connections_from.get(key)
        return references if references else []

    def get_connections_referenced_by(self, kind: ConnectionKind, managed_object_index: int) -> List[JointConnection]:
        key = self.get_connection_key(kind=kind, index=managed_object_index)
        references = self.connections_to.get(key)
        return references if references else []

    def get_connections_in_heap_section(self, section: MemorySection) -> List[JointConnection]:
        if not section.bytes: return []
        start_address = section.startAddress
        stop_address = start_address + len(section.bytes)
        references = []
        for n in range(len(self.managed_objects)):
            mo = self.managed_objects[n]
            if start_address <= mo.address < stop_address:
                references.append(JointConnection(dst=n, dst_kind=ConnectionKind.managed))
        return references

    def find_mono_script_type(self, native_index: int) -> Tuple[str, int]:
        key = self.get_connection_key(kind=ConnectionKind.native, index=native_index)
        reference_list = self.connections_from.get(key)
        if reference_list:
            for refer in reference_list:
                type_index = self.snapshot.nativeObjects[refer.dst].nativeTypeArrayIndex
                if refer.dst_kind == ConnectionKind.native and type_index == self.__nt_index.MonoScript:
                    script_name = self.snapshot.nativeObjects[refer.dst].name
                    return script_name, type_index
        return '', -1

    def is_enum(self, type: TypeDescription):
        if not type.isValueType: return False
        if type.baseOrElementTypeIndex == -1: return False
        return type.baseOrElementTypeIndex == self.__mt_index.system_Enum

    def is_subclass_of_native_type(self, type: PackedNativeType, base_type_index: int):
        if type.typeIndex == base_type_index:
            return True
        if base_type_index < 0 or type.typeIndex < 0:
            return False
        while type.nativeBaseTypeArrayIndex != -1:
            if type.nativeBaseTypeArrayIndex == base_type_index:
                return True
            type = self.snapshot.nativeTypes[type.nativeBaseTypeArrayIndex]
        return False

    def is_subclass_of_managed_type(self, type: TypeDescription, base_type_index: int):
        if type.typeIndex == base_type_index:
            return True
        if type.typeIndex < 0 or base_type_index < 0:
            return False
        while type.baseOrElementTypeIndex != -1:
            if type.typeIndex == base_type_index:
                return True
            type = self.snapshot.typeDescriptions[type.baseOrElementTypeIndex]
        return False

    def contain_references(self, type_index: int):
        mt = self.snapshot.typeDescriptions[type_index]
        if not mt.isValueType: return True
        managed_types = self.snapshot.typeDescriptions
        type_count = len(managed_types)
        for field in mt.fields:
            if field.typeIndex < 0 or field.typeIndex >= type_count: continue
            field_type = managed_types[field.typeIndex]
            if not field_type.isValueType: return True
        return False

    def try_connect_with_native_object(self, managed_object: UnityManagedObject):
        if managed_object.native_object_index >= 0: return
        mt = self.snapshot.typeDescriptions[managed_object.type_index]
        if mt.isValueType or mt.isArray or not mt.isUnityEngineObjectType: return
        native_address = self.__heap_reader.read_pointer(managed_object.address + self.__cached_ptr.offset)
        if native_address == 0: return
        native_object_index = self.find_native_object_at_address(address=native_address)
        if native_object_index == -1: return
        # connect native object and managed object
        native_type_index = self.snapshot.nativeObjects[native_object_index].nativeTypeArrayIndex
        managed_object.native_object_index = native_object_index
        self.snapshot.nativeObjects[native_object_index].managedObjectArrayIndex = managed_object.managed_object_index
        # connect native type and managed type
        self.snapshot.typeDescriptions[managed_object.type_index].nativeTypeArrayIndex = native_type_index
        self.snapshot.nativeTypes[native_type_index].managedTypeArrayIndex = managed_object.type_index

    def set_object_size(self, managed_object: UnityManagedObject, type: TypeDescription, memory_reader:HeapReader):
        if managed_object.size == 0:
            managed_object.size = memory_reader.read_object_size(
                address=managed_object.address, type=type
            )

    def create_managed_object(self, address:int, type_index:int)->UnityManagedObject:
        mo = UnityManagedObject()
        mo.address = address
        mo.type_index = type_index
        mo.managed_object_index = len(self.managed_objects)
        self.try_connect_with_native_object(managed_object=mo)
        self.__visit[mo.address] = mo.managed_object_index
        self.managed_objects.append(mo)
        return mo

    def is_crawlable(self, type:TypeDescription)->bool:
        return not type.isValueType or type.size > 8 # self.vm.pointerSize

    def crawl_managed_array_address(self, address:int, type:TypeDescription, memory_reader:HeapReader, joint:KeepAliveJoint, depth:int):
        is_static_crawling = isinstance(memory_reader, StaticFieldReader)
        if address < 0 or not is_static_crawling and address == 0: return
        element_type = self.snapshot.typeDescriptions[type.baseOrElementTypeIndex]
        if not self.is_crawlable(type=element_type): return
        element_count = memory_reader.read_array_length(address=address, type=type)
        for n in range(element_count):
            if element_type.isValueType:
                element_address = address + self.vm.arrayHeaderSize + n * element_type.size - self.vm.objectHeaderSize
            else:
                address_ptr = address + self.vm.arrayHeaderSize + n * self.vm.pointerSize
                element_address = memory_reader.read_pointer(address=address_ptr)
            # if address == 0x14b023960:
            #     print('++ {:x} {}'.format(element_address, element_type))
            self.crawl_managed_entry_address(address=element_address, type=element_type, memory_reader=memory_reader,
                                             joint=joint.clone(array_index=n), depth=depth+1)

    def crawl_managed_entry_address(self, address:int, type:TypeDescription, memory_reader:HeapReader, joint:KeepAliveJoint = None, depth = 0):
        is_static_crawling = isinstance(memory_reader, StaticFieldReader)
        if address < 0 or not is_static_crawling and address == 0: return
        if depth >= 512:
            print('!![ITER_MAX={}] address={:08x} type={}'.format(depth, address, type))
            return
        if type and type.isValueType:
            type_index = type.typeIndex
        else:
            if address in self.__visit: return
            type_index = self.find_type_of_address(address=address)
            if type_index == -1:
                type_index = type.typeIndex if type else -1
        if type_index == -1: return
        entry_type = self.snapshot.typeDescriptions[type_index]
        mo = self.create_managed_object(address=address, type_index=type_index)
        mo.handle_index = joint.handle_index
        assert mo and mo.managed_object_index >= 0, mo
        self.set_object_size(managed_object=mo, type=self.snapshot.typeDescriptions[type_index], memory_reader=memory_reader)
        print(mo)
        if not entry_type.isValueType:
            self.__visit[mo.address] = mo.managed_object_index
        if joint.handle_index >= 0:
            connection = JointConnection(src_kind=ConnectionKind.handle, src=joint.object_index,
                                         dst_kind=ConnectionKind.managed, dst=mo.managed_object_index, joint=joint)
        else:
            if joint.is_static:
                connection = JointConnection(src_kind=ConnectionKind.static, src=-1,
                                             dst_kind=ConnectionKind.managed, dst=mo.managed_object_index, joint=joint)
            else:
                connection = JointConnection(src_kind=ConnectionKind.managed, src=joint.object_index,
                                             dst_kind=ConnectionKind.managed, dst=mo.managed_object_index, joint=joint)
        self.accept_connection(connection=connection)
        if entry_type.isArray: # crawl array
            self.crawl_managed_array_address(address=address, type=entry_type, memory_reader=memory_reader, joint=joint, depth=depth+1)
            return
        member_joint = KeepAliveJoint(object_type_index=type_index, object_index=mo.managed_object_index)
        dive_type = entry_type
        # if address == 0x13f052a48:
        #     print('## {} {}'.format(self.find_type_of_address(address=address), entry_type))
        while dive_type:
            for field in dive_type.fields: # crawl fields
                field_type = self.snapshot.typeDescriptions[field.typeIndex]
                if is_static_crawling:
                    if field_type.isValueType and field.typeIndex == dive_type.typeIndex: continue
                else:
                    if field.isStatic: continue
                # if address == 0x13f052a48: print('+', field.typeIndex, entry_type.typeIndex, field, field_type)
                if not self.is_crawlable(type=field_type): continue
                if field_type.isValueType:
                    field_address = address + field.offset - self.vm.objectHeaderSize
                else:
                    if is_static_crawling:
                        address_ptr = address + field.offset - self.vm.objectHeaderSize
                    else:
                        address_ptr = address + field.offset
                    try:
                        field_address = memory_reader.read_pointer(address=address_ptr)
                    except: continue
                # if address == 0x13f052a48 and field.offset == 16:
                #     print('{} {:x} {:x} {}'.format(field, address, field_address, address_ptr))
                pass_field_type = field_type if field_type.isValueType else None
                pass_memory_reader = memory_reader if field_type.isValueType else self.__heap_reader
                self.crawl_managed_entry_address(address=field_address, type=pass_field_type, memory_reader=pass_memory_reader,
                                                 joint=member_joint.clone(field_type_index=field_type.typeIndex, field_index=field.slotIndex), depth=depth+1)
            dive_type = self.snapshot.typeDescriptions[dive_type.baseOrElementTypeIndex] if dive_type.baseOrElementTypeIndex >= 0 else None

    def crawl_handles(self):
        for item in self.snapshot.gcHandles:
            self.crawl_managed_entry_address(address=item.target, joint=KeepAliveJoint(handle_index=item.gcHandleArrayIndex),
                                             memory_reader=self.__heap_reader, type=None)

    def crawl_static(self):
        managed_types = self.snapshot.typeDescriptions
        static_reader = StaticFieldReader(snapshot=self.snapshot)
        for mt in managed_types:
            if not mt.staticFieldBytes: continue
            for n in range(len(mt.fields)):
                field = mt.fields[n]
                if not field.isStatic: continue
                static_reader.load(memory=mt.staticFieldBytes)
                field_type = managed_types[field.typeIndex]
                if field_type.isValueType:
                    field_address = field.offset - self.vm.objectHeaderSize
                    memory_reader = static_reader
                else:
                    address_ptr = field.offset
                    try:
                        field_address = static_reader.read_pointer(address=address_ptr)
                    except: continue
                    memory_reader = self.__heap_reader
                # if mt.typeIndex == 440 and field.offset == 8:
                #     memory_reader.debug()
                #     print('##', field, field_address)
                self.crawl_managed_entry_address(address=field_address, type=field_type, memory_reader=memory_reader,
                                                 joint=KeepAliveJoint(object_type_index=mt.typeIndex, field_index=field.typeIndex, field_type_index=field.slotIndex, is_static=True))





























