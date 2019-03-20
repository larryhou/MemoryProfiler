from .core import *
from .heap import *
from typing import List, Dict, Tuple
import enum


class ConnectionKind(enum.Enum):
    none, handle, native, managed, static = range(5)

class KeepAliveJoint(object):
    def __init__(self, type_index:int = -1, object_index:int = -1, field_type_index:int = -1, field_index:int = -1, array_index:int = -1, handle_index:int = -1, is_static:bool = False):
        # gcHandle joint
        self.handle_index:int = handle_index
        # managed object joint
        self.type_index: int = type_index
        self.object_index: int = object_index
        # reference point
        self.field_type_index:int = field_type_index
        self.field_index:int = field_index
        self.array_index:int = array_index
        self.is_static = is_static

    def clone(self, field_type_index:int = -1, field_index = -1, array_index:int = -1)-> 'KeepAliveJoint':
        rj = KeepAliveJoint()
        rj.handle_index = self.handle_index
        rj.type_index = self.type_index
        rj.object_index = self.object_index
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

class ManagedObject(object):
    def __init__(self):
        self.address: int = -1
        self.memory: bytes = None
        self.type_index: int = -1
        self.native_object_index: int = -1
        self.managed_object_index: int = -1
        self.handle_index: int = -1
        self.size: int = 0
        self.static_bytes:bytes = b''

class MemorySnapshotCrawler(object):
    def __init__(self, snapshot: PackedMemorySnapshot):
        self.managed_objects: List[ManagedObject] = []
        self.visit: Dict[int, int] = {}
        self.snapshot: PackedMemorySnapshot = snapshot
        self.snapshot.preprocess()
        self.heap_reader: HeapReader = HeapReader(snapshot=snapshot)
        self.cached_ptr: FieldDescription = snapshot.cached_ptr
        self.vm = snapshot.virtualMachineInformation

        # type index utils
        self.nt_index = self.snapshot.nativeTypeIndex
        self.mt_index = self.snapshot.managedTypeIndex

        # connections
        self.managed_connections: List[JointConnection] = []
        self.mono_script_connections: List[JointConnection] = []
        self.connections_from: Dict[int, List[JointConnection]] = []
        self.connections_to: Dict[int, List[JointConnection]] = []

        self.type_address_map: Dict[int, int] = {}
        self.native_object_address_map: Dict[int, int] = {}
        self.managed_object_address_map: Dict[int, int] = {}
        self.managed_native_address_map: Dict[int, int] = {}
        self.handle_address_map: Dict[int, int] = {}

    def crawl(self):
        self.init_managed_types()
        self.init_managed_connections()
        self.init_mono_script_connections()
        self.init_managed_fields()
        self.crawl_handles()
        self.crawl_static()

    def init_managed_fields(self):
        managed_types = self.snapshot.typeDescriptions
        for mt in managed_types:
            for field in mt.fields:
                offset = field.name.rfind('>k__BackingField')
                if offset >= 0:
                    field.name = field.name[1:offset]

    def init_mono_script_connections(self):
        for n in range(len(self.managed_connections)):
            connection = self.managed_connections[n]
            if connection.dst_kind == ConnectionKind.native \
                    and self.snapshot.nativeObjects[connection.dst].nativeTypeArrayIndex == self.nt_index.MonoScript:
                self.mono_script_connections.append(connection)

    def init_managed_types(self):
        managed_types = self.snapshot.typeDescriptions
        for n in range(len(managed_types)):
            mt = managed_types[n]
            mt.isUnityEngineObjectType = self.is_subclass_of_managed_type(mt, self.mt_index.unityengine_Object)

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
            self.add_connection(connection=connection)
        self.managed_connections = managed_connections

    def add_connection(self, connection:JointConnection):
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

        vtable_ptr = self.heap_reader.read_pointer(address)
        if vtable_ptr == 0: return -1

        class_ptr = self.heap_reader.read_pointer(vtable_ptr)
        if class_ptr == 0:
            return self.find_type_at_type_info_address(vtable_ptr)
        return self.find_type_at_type_info_address(class_ptr)

    def find_type_at_type_info_address(self, type_info_address: int) -> int:
        if type_info_address == 0: return -1
        if not self.type_address_map:
            managed_types = self.snapshot.typeDescriptions
            for t in managed_types:
                self.type_address_map[t.typeInfoAddress] = t.typeIndex
        type_index = self.type_address_map.get(type_info_address)
        return -1 if type_index is None else type_index

    def find_managed_object_of_native_object(self, native_address: int):
        if native_address == 0: return -1
        if not self.managed_native_address_map:
            for n in range(len(self.managed_objects)):
                mo = self.managed_objects[n]
                if mo.native_object_index >= 0:
                    no = self.snapshot.nativeObjects[mo.native_object_index]
                    self.managed_native_address_map[no.nativeObjectAddress] = n
        manage_index = self.managed_native_address_map.get(native_address)
        return -1 if manage_index is None else manage_index

    def find_managed_object_at_address(self, address: int) -> int:
        if address == 0: return -1
        if not self.managed_object_address_map:
            for n in range(len(self.managed_objects)):
                mo = self.managed_objects[n]
                self.managed_object_address_map[mo.address] = n
        object_index = self.managed_object_address_map.get(address)
        return -1 if object_index is None else object_index

    def find_native_object_at_address(self, address: int):
        if address == 0: return -1
        if not self.native_object_address_map:
            for n in range(len(self.snapshot.nativeObjects)):
                no = self.snapshot.nativeObjects[n]
                self.native_object_address_map[no.nativeObjectAddress] = n
        object_index = self.native_object_address_map.get(address)
        return -1 if object_index is None else object_index

    def find_handle_with_target_address(self, address: int):
        if address == 0: return -1
        if not self.handle_address_map:
            for n in range(len(self.snapshot.gcHandles)):
                handle = self.snapshot.gcHandles[n]
                self.handle_address_map[handle.target] = n
        handle_index = self.handle_address_map.get(address)
        return -1 if handle_index is None else handle_index

    def get_connections_of(self, kind: ConnectionKind, index: int) -> List[JointConnection]:
        key = self.get_connection_key(kind=kind, index=index)
        references = self.connections_from.get(key)
        return references if references else []

    def get_connections_referenced_by(self, kind: ConnectionKind, index: int) -> List[JointConnection]:
        key = self.get_connection_key(kind=kind, index=index)
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
                if refer.dst_kind == ConnectionKind.native and type_index == self.nt_index.MonoScript:
                    script_name = self.snapshot.nativeObjects[refer.dst].name
                    return script_name, type_index
        return '', -1

    def is_enum(self, type: TypeDescription):
        if not type.isValueType: return False
        if type.baseOrElementTypeIndex == -1: return False
        return type.baseOrElementTypeIndex == self.mt_index.system_Enum

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

    def try_connect_with_native_object(self, managed_object: ManagedObject):
        if managed_object.native_object_index >= 0: return
        mt = self.snapshot.typeDescriptions[managed_object.type_index]
        if mt.isValueType or mt.isArray or not mt.isUnityEngineObjectType: return
        native_address = self.heap_reader.read_pointer(managed_object.address + self.cached_ptr.offset)
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

    def set_object_size(self, managed_object: ManagedObject, type: TypeDescription):
        if managed_object.size == 0:
            managed_object.size = self.heap_reader.read_object_size(
                address=managed_object.address, type=type
            )

    def create_managed_object(self, address:int, type_index:int)->ManagedObject:
        mo = ManagedObject()
        mo.address = address
        mo.type_index = type_index
        mo.managed_object_index = len(self.managed_objects)
        self.try_connect_with_native_object(managed_object=mo)
        self.set_object_size(managed_object=mo, type=self.snapshot.typeDescriptions[type_index])
        self.visit[mo.address] = mo.managed_object_index
        self.managed_objects.append(mo)
        return mo

    def is_crawlable(self, type:TypeDescription)->bool:
        return not type.isValueType or type.size >= self.vm.pointerSize + self.vm.objectHeaderSize

    def crawl_managed_array_address(self, address:int, type:TypeDescription, memory_reader:HeapReader, joint:KeepAliveJoint):
        if address <= 0 or address in self.visit: return
        element_type = self.snapshot.typeDescriptions[type.baseOrElementTypeIndex]
        if self.is_crawlable(type=element_type): return
        element_count = memory_reader.read_array_length(address=address, type=type)
        for n in range(element_count):
            if element_type.isValueType:
                element_address = memory_reader.read_pointer(address=address + n * element_type.size + self.vm.arrayHeaderSize - self.vm.objectHeaderSize)
            else:
                element_address = memory_reader.read_pointer(address=address + n * self.vm.pointerSize + self.vm.arrayHeaderSize)
            self.crawl_managed_entry_address(address=element_address, type=element_type, memory_reader=memory_reader,
                                             joint=joint.clone(array_index=n))

    def crawl_managed_entry_address(self, address:int, type:TypeDescription, memory_reader:HeapReader, joint:KeepAliveJoint = None):
        if address <= 0 or address in self.visit: return
        if not type:
            type_index = self.find_type_of_address(address=address)
            if type_index == -1: return
            entry_type = self.snapshot.typeDescriptions[type_index]
        else:
            entry_type = type
            type_index = entry_type.typeIndex

        if not entry_type.isValueType: # crawl reference
            mo = self.create_managed_object(address=address, type_index=type_index)
            assert mo and mo.managed_object_index >= 0
            self.visit[mo.address] = mo.managed_object_index
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
            self.add_connection(connection=connection)
            if entry_type.isArray: # crawl array
                self.crawl_managed_array_address(address=address, type=entry_type, memory_reader=memory_reader, joint=joint)
                return
        for field in entry_type.fields: # crawl fields
            field_type = self.snapshot.typeDescriptions[field.typeIndex]
            if not self.is_crawlable(type=field_type): continue
            if field_type.isValueType:
                field_address = address + field.offset - self.vm.objectHeaderSize
            elif isinstance(memory_reader, StaticFieldReader):
                field_address = address + field.offset - self.vm.objectHeaderSize
            else:
                field_address = address + field.offset
            self.crawl_managed_entry_address(address=field_address, type=field_type, memory_reader=memory_reader,
                                             joint=joint.clone(field_type_index=field_type.typeIndex, field_index=field.slotIndex))

    def crawl_handles(self):
        for item in self.snapshot.gcHandles:
            self.crawl_managed_entry_address(address=item.target, joint=KeepAliveJoint(handle_index=item.gcHandleArrayIndex),
                                             memory_reader=self.heap_reader, type=None)

    def crawl_static(self):
        managed_types = self.snapshot.typeDescriptions
        static_reader = StaticFieldReader(snapshot=self.snapshot)
        for mt in managed_types:
            if not mt.staticFieldBytes: continue
            for n in range(len(mt.fields)):
                field = mt.fields[n]
                if not field.isStatic: continue
                static_reader.load(memory=mt.staticFieldBytes)
                self.crawl_managed_entry_address(address=field.offset, type=managed_types[field.typeIndex], memory_reader=static_reader,
                                                 joint=KeepAliveJoint(type_index=mt.typeIndex, field_index=field.typeIndex, field_type_index=field.slotIndex, is_static=True))




























