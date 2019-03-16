from .core import *
from .heap import *
from typing import List, Dict, Tuple
import enum


class ConnectionKind(enum.Enum):
    none, handle, native, managed, static = range(5)


class ManagedConnection(object):
    def __init__(self, from_ = 0, from_kind = ConnectionKind.none, to = 0, to_kind = ConnectionKind.none):
        self.from_: int = from_
        self.to: int = to
        self.from_kind: ConnectionKind = from_kind
        self.to_kind: ConnectionKind = to_kind


class ManagedObject(object):
    def __init__(self):
        self.address: int = -1
        self.memory: bytes = None
        self.type_index: int = -1
        self.native_object_index: int = -1
        self.manage_object_index: int = -1
        self.handle_index: int = -1
        self.size: int = 0


class ManagedStaticField(object):
    def __init__(self):
        self.object_type_index: int = -1
        self.field_index: int = -1
        self.static_object_index = -1


class MemorySnapshotCrawler(object):
    def __init__(self, snapshot: PackedMemorySnapshot):
        self.managed_objects: List[ManagedObject] = []

        self.crawled: List[ManagedObject] = []
        self.static_fields: List[ManagedStaticField] = []
        self.visit: Dict[int, int] = {}
        self.snapshot: PackedMemorySnapshot = snapshot
        self.heap_reader: HeapReader = HeapReader(snapshot=snapshot)
        self.cached_ptr: TypeDescription = snapshot.cached_ptr
        self.vm = snapshot.virtualMachineInformation

        # connections
        self.managed_connections: List[ManagedConnection] = []
        self.connections_from: Dict[int, List[ManagedConnection]] = []
        self.connections_to: Dict[int, List[ManagedConnection]] = []

        self.type_address_map: Dict[int, int] = {}
        self.native_object_address_map: Dict[int, int] = {}
        self.manage_object_address_map: Dict[int, int] = {}
        self.manage_native_address_map: Dict[int, int] = {}
        self.handle_address_map: Dict[int, int] = {}

    def crawl(self):
        self.create_managed_connections()
        self.crawl_hanles()
        self.crawl_static()
        self.crawl_managed_objects()

    def create_managed_connections(self, exclude_native: bool = False):
        managed_start = 0
        managed_stop = managed_start + len(self.snapshot.gcHandles)
        native_start = managed_start + managed_stop
        native_stop = native_start + len(self.snapshot.nativeObjects)
        managed_connections = []
        for it in self.snapshot.connections:
            mc = ManagedConnection(from_=it.from_, to=it.to)
            mc.from_kind = ConnectionKind.handle
            if native_start <= mc.from_ < native_stop:
                mc.from_ -= native_start
                mc.from_kind = ConnectionKind.native
            mc.to_kind = ConnectionKind.handle
            if native_start <= mc.to < native_stop:
                mc.to -= native_start
                mc.to_kind = ConnectionKind.native
            if exclude_native and mc.from_kind == ConnectionKind.native: continue
            managed_connections.append(mc)
        self.managed_connections = managed_connections

    def add_connection(self, from_: int, from_kind: ConnectionKind, to: int, to_kind: ConnectionKind):
        mc = ManagedConnection(from_=from_, from_kind=from_kind, to=to, to_kind=to_kind)
        if mc.from_kind != ConnectionKind.none and mc.from_ != -1:
            key = self.hash_connection(kind=mc.from_kind, index=mc.from_)
            if key not in self.connections_from:
                self.connections_from[key] = []
            self.connections_from[key].append(mc)
        if mc.to_kind != ConnectionKind.none and mc.to != -1:
            if mc.to < 0: mc.to = -mc.to
            key = self.hash_connection(kind=mc.to_kind, index=mc.to)
            if key not in self.connections_to:
                self.connections_to[key] = []
            self.connections_to[key].append(mc)

    def hash_connection(self, kind: ConnectionKind, index: int):
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

    def find_manage_object_of_native_object(self, native_address:int):
        if native_address == 0: return -1
        if not self.manage_native_address_map:
            for n in range(len(self.managed_objects)):
                mo = self.managed_objects[n]
                if mo.native_object_index >= 0:
                    no = self.snapshot.nativeObjects[mo.native_object_index]
                    self.manage_native_address_map[no.nativeObjectAddress] = n
        manage_index = self.manage_native_address_map.get(native_address)
        return -1 if manage_index is None else manage_index

    def find_manage_object_at_address(self, address: int) -> int:
        if address == 0: return -1
        if not self.manage_object_address_map:
            for n in range(len(self.managed_objects)):
                mo = self.managed_objects[n]
                self.manage_object_address_map[mo.address] = n
        object_index = self.manage_object_address_map.get(address)
        return -1 if object_index is None else object_index

    def find_native_object_at_address(self, address: int):
        if address == 0: return -1
        if not self.native_object_address_map:
            for n in range(len(self.snapshot.nativeObjects)):
                no = self.snapshot.nativeObjects[n]
                self.native_object_address_map[no.nativeObjectAddress] = n
        object_index = self.native_object_address_map.get(address)
        return -1 if object_index is None else object_index

    def find_handle_with_target_address(self, address:int):
        if address == 0: return -1
        if not self.handle_address_map:
            for n in range(len(self.snapshot.gcHandles)):
                handle = self.snapshot.gcHandles[n]
                self.handle_address_map[handle.target] = n
        handle_index = self.handle_address_map.get(address)
        return -1 if handle_index is None else handle_index

    def get_connections_of(self, kind:ConnectionKind, index:int)->List[ManagedConnection]:
        key = self.hash_connection(kind=kind, index=index)
        references = self.connections_from.get(key)
        return references if references else []

    def get_connections_referenced_by(self, kind:ConnectionKind, index:int)->List[ManagedConnection]:
        key = self.hash_connection(kind=kind, index=index)
        references = self.connections_to.get(key)
        return references if references else []

    def get_connections_in_heap_section(self, section:MemorySection)->List[ManagedConnection]:
        if not section.bytes: return []
        start_address = section.startAddress
        stop_address = start_address + len(section.bytes)
        references = []
        for n in range(len(self.managed_objects)):
            mo = self.managed_objects[n]
            if start_address <= mo.address < stop_address:
                references.append(ManagedConnection(to=n, to_kind=ConnectionKind.managed))
        return references

    def find_mono_script_type(self, native_index: int)->Tuple[str, int]:
        key = self.hash_connection(kind=ConnectionKind.native, index=native_index)
        mc_list = self.connections_from.get(key)
        if mc_list:
            for mc in mc_list:
                type_index = self.snapshot.nativeObjects[mc.to].nativeTypeArrayIndex

                if mc.to_kind == ConnectionKind.native \
                        and self.snapshot.typeDescriptions[type_index].name == 'UnityEditor.MonoScript':
                    script_name = self.snapshot.nativeObjects[mc.to].name
                    return script_name, type_index
        return '', -1

    def is_enum(self, type: TypeDescription):
        if not type.isValueType: return False
        if type.baseOrElementTypeIndex == -1: return False
        base_type = self.snapshot.typeDescriptions[type.baseOrElementTypeIndex]
        return base_type.name == 'System.Enum'

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

    def is_subclass_of_manage_type(self, type: TypeDescription, base_type_index: int):
        if type.typeIndex == base_type_index:
            return True
        if type.typeIndex < 0 or base_type_index < 0:
            return False
        while type.baseOrElementTypeIndex != -1:
            if type.typeIndex == base_type_index:
                return True
            type = self.snapshot.typeDescriptions[type.baseOrElementTypeIndex]
        return False

    def try_connect_native_object(self, managed_object: ManagedObject):
        if managed_object.native_object_index >= 0: return
        object_type = self.snapshot.typeDescriptions[managed_object.type_index]
        if object_type.isValueType or object_type.isArray: return

    def set_object_size(self, managed_object: ManagedObject, object_type: TypeDescription):
        if managed_object.size == 0:
            managed_object.size = self.heap_reader.read_object_size(
                address=managed_object.address, object_type=object_type
            )

    def try_create_managed_object(self, address: int) -> int:
        type_index = self.find_type_of_address(address)
        if type_index == -1:
            return -1
        mo = ManagedObject()
        mo.address = address
        mo.type_index = type_index
        mo.manage_object_index = len(self.managed_objects)
        self.try_connect_native_object(managed_object=mo)
        self.set_object_size(managed_object=mo, object_type=self.snapshot.typeDescriptions[type_index])
        self.visit[mo.address] = mo.manage_object_index
        self.managed_objects.append(mo)
        self.crawled.append(mo)
        return mo.manage_object_index

    def crawl_hanles(self):
        handle_list = self.snapshot.gcHandles
        for n in range(len(handle_list)):
            item = handle_list[n]
            if item.target == 0: continue

    def crawl_static(self):
        pass

    def crawl_managed_objects(self):
        pass
