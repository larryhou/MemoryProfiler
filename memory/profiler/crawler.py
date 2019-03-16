from .core import *
from .heap import *
from typing import List, Dict

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
        self.type_address_map:Dict[int, int] = {}
        self.vm = snapshot.virtualMachineInformation

    def crawl(self):
        self.crawl_hanles()
        self.crawl_static()
        self.crawl_managed_objects()

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

