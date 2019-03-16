from .core import *
from .heap import *
from typing import List, Dict
import enum

class ConnectionKind(enum.Enum):
    none, handle, native, managed, static = range(5)

class ManagedConnection(object):
    def __init__(self):
        self.from_:int = 0
        self.to:int = 0
        self.from_kind:ConnectionKind = ConnectionKind.none
        self.to_kind:ConnectionKind = ConnectionKind.none

class ManagedObject(object):
    def __init__(self):
        self.address:int = -1
        self.memory:bytes = None
        self.object_type_index:int = -1
        self.native_type_index:int = -1
        self.object_index:int = -1
        self.handle_index:int = -1
        self.size:int = 0

class ManagedStaticField(object):
    def __init__(self):
        self.object_type_index:int = -1
        self.field_index:int = -1
        self.static_object_index = -1

class MemorySnapshotCrawler(object):
    def __init__(self, snapshot:PackedMemorySnapshot):
        self.managed_objects:List[ManagedObject] = []

        self.crawled:List[ManagedObject] = []
        self.static_fields:List[ManagedStaticField] = []
        self.visit:Dict[int, int] = {}
        self.snapshot:PackedMemorySnapshot = snapshot
        self.heap_reader:HeapReader = HeapReader(snapshot=snapshot)
        self.cached_ptr:TypeDescription = snapshot.cached_ptr
        self.vm = snapshot.virtualMachineInformation

        # connections
        self.managed_connections: List[ManagedConnection] = []
        self.connections_from:Dict[int, List[ManagedConnection]] = []
        self.connections_to:Dict[int, List[ManagedConnection]] = []

        self.type_address_map:Dict[int, int] = {}
        self.native_object_address_map:Dict[int, int] = {}
        self.managed_object_address_map:Dict[int, int] = {}


    def crawl(self):
        self.create_managed_connections()
        self.crawl_hanles()
        self.crawl_static()
        self.crawl_managed_objects()

    def create_managed_connections(self, exclude_native:bool = False):
        managed_start = 0
        managed_stop = managed_start + len(self.snapshot.gcHandles)
        native_start = managed_start + managed_stop
        native_stop = native_start + len(self.snapshot.nativeObjects)
        managed_connections = []
        for it in self.snapshot.connections:
            mc = ManagedConnection()
            mc.from_ = it.from_
            mc.to = it.to
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

    def add_connection(self, from_:int, from_kind:ConnectionKind, to:int, to_kind:ConnectionKind):
        mc = ManagedConnection()
        mc.from_, mc.from_kind = from_, from_kind
        mc.to, mc.to_kind = to, to_kind
        if mc.from_kind != ConnectionKind.none and mc.from_ != -1:
            hash = self.hash_connection(kind=mc.from_kind, index=mc.from_)
            if hash not in self.connections_from:
                self.connections_from[hash] = []
            self.connections_from[hash].append(mc)
        if mc.to_kind != ConnectionKind.none and mc.to != -1:
            if mc.to < 0: mc.to = -mc.to
            hash = self.hash_connection(kind=mc.to_kind, index=mc.to)
            if hash not in self.connections_to:
                self.connections_to[hash] = []
            self.connections_to[hash].append(mc)

    def hash_connection(self, kind:ConnectionKind, index:int):
        return kind.value << 50 + index

    def find_type_of_address(self, address:int)->int:
        type_index = self.find_type_of_type_info_address(type_info_address=address)
        if type_index != -1: return type_index

        vtable_ptr = self.heap_reader.read_pointer(address)
        if vtable_ptr == 0: return -1

        class_ptr = self.heap_reader.read_pointer(vtable_ptr)
        if class_ptr == 0:
            return self.find_type_of_type_info_address(vtable_ptr)
        return self.find_type_of_type_info_address(class_ptr)

    def find_type_of_type_info_address(self, type_info_address:int)->int:
        if type_info_address == 0: return -1
        if not self.type_address_map:
            managed_types = self.snapshot.typeDescriptions
            for t in managed_types:
                self.type_address_map[t.typeInfoAddress] = t.typeIndex
        type_index = self.type_address_map.get(type_info_address)
        return type_index if type_index else -1

    def find_managed_object_at_address(self, address:int)->int:
        return 0

    def find_native_object_at_address(self, address:int)->int:
        return 0

    def try_connect_native_object(self, managed_object:ManagedObject):
        if managed_object.native_type_index >= 0: return
        object_type = self.snapshot.typeDescriptions[managed_object.object_type_index]
        if object_type.isValueType or object_type.isArray: return

    def set_object_size(self, managed_object:ManagedObject, object_type:TypeDescription):
        if managed_object.size == 0:
            managed_object.size = self.heap_reader.read_object_size(
                address=managed_object.address, object_type=object_type
            )

    def try_create_managed_object(self, address:int)->int:
        type_index = self.find_type_of_address(address)
        if type_index == -1:
            return -1
        mo = ManagedObject()
        mo.address = address
        mo.object_type_index = type_index
        mo.object_index = len(self.managed_objects)
        self.try_connect_native_object(managed_object=mo)
        self.set_object_size(managed_object=mo, object_type=self.snapshot.typeDescriptions[type_index])
        self.visit[mo.address] = mo.object_index
        self.managed_objects.append(mo)
        self.crawled.append(mo)
        return mo.object_index

    def crawl_hanles(self):
        handle_list = self.snapshot.gcHandles
        for n in range(len(handle_list)):
            item = handle_list[n]
            if item.target == 0: continue


    def crawl_static(self):
        pass

    def crawl_managed_objects(self):
        pass

