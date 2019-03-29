from .core import *
from .heap import *
from typing import List, Dict, Tuple
import enum, io

class BridgeKind(enum.Enum):
    none, handle, native, managed, static = range(5)

class ActiveJoint(object):
    def __init__(self, object_type_index:int = -1, object_index:int = -1, object_address:int = 0,
                 field_type_index:int = -1, field_index:int = -1, field_offset:int = -1, field_address:int = 0,
                 array_index:int = -1, handle_index:int = -1, is_static:bool = False):
        # gcHandle joint
        self.handle_index:int = handle_index
        # managed object joint
        self.object_type_index: int = object_type_index
        self.object_index: int = object_index
        self.object_address:int = object_address
        # reference point
        self.field_type_index:int = field_type_index
        self.field_index:int = field_index
        self.field_offset:int = field_offset
        self.field_address:int = field_address
        self.array_index:int = array_index
        self.is_static = is_static

    def clone(self, object_type_index:int = -1, object_index:int = -1, object_address:int = 0,
                 field_type_index:int = -1, field_index:int = -1, field_offset:int = -1, field_address:int = 0,
                 array_index:int = -1, handle_index:int = -1)-> 'ActiveJoint':
        rj = ActiveJoint()
        rj.handle_index = handle_index if handle_index >= 0 else self.handle_index
        rj.object_type_index = object_type_index if object_type_index >= 0 else self.object_type_index
        rj.object_index = object_index if object_index >= 0 else self.object_index
        rj.object_address = object_address if object_address > 0 else self.object_address
        rj.field_type_index = field_type_index if field_type_index >= 0 else self.field_type_index
        rj.field_index = field_index if field_index >= 0 else self.field_index
        rj.array_index = array_index if array_index >= 0 else self.array_index
        rj.field_offset = field_offset if field_offset >= 0 else self.field_offset
        rj.field_address = field_address if field_address > 0 else self.field_address
        rj.is_static = self.is_static
        return rj

    def __repr__(self):
        return '[ActiveJoint] object_index={} object_type_index={} object_address={:x} field_index={} field_type_index={} field_offset={} field_address={:x} array_index={} handle_index={} is_static={}'.format(
            self.object_index, self.object_type_index, self.object_address, self.field_index,self.field_type_index, self.field_offset, self.field_address, self.array_index, self.handle_index, self.is_static
        )

class JointBridge(object):
    def __init__(self, src=0, src_kind=BridgeKind.none, dst=0, dst_kind=BridgeKind.none, joint:ActiveJoint = None):
        self.src: int = src
        self.dst: int = dst
        self.src_kind: BridgeKind = src_kind
        self.dst_kind: BridgeKind = dst_kind
        self.joint:ActiveJoint = joint

    def __repr__(self):
        return '[JointBridge] from={} from_kind={} to={} to_kind={}'.format(
            self.src, self.src_kind.name, self.dst, self.dst_kind.name
        )

class UnityManagedObject(object):
    def __init__(self):
        self.address: int = -1
        self.type_index: int = -1
        self.managed_object_index: int = -1
        self.native_object_index: int = -1
        self.handle_index: int = -1
        self.is_value_type: bool = False
        self.size: int = 0
        self.native_size:int = 0
        self.joint:ActiveJoint = None

    def __repr__(self):
        return '[UnityManagedObject] address={:08x} type_index={} native_object_index={} managed_object_index={} handle_index={} size={} native_size={:,}'.format(
            self.address, self.type_index, self.native_object_index, self.managed_object_index, self.handle_index, self.size, self.native_size
        )

class MemorySnapshotCrawler(object):
    def __init__(self, snapshot: PackedMemorySnapshot):
        self.managed_objects: List[UnityManagedObject] = []
        self.snapshot: PackedMemorySnapshot = snapshot
        self.snapshot.initialize()
        self.vm = snapshot.virtualMachineInformation
        self.heap_memory: HeapReader = HeapReader(snapshot=snapshot)

        # record crawling footprint
        self.__visit: Dict[int, int] = {}
        self.__cached_ptr: FieldDescription = snapshot.cached_ptr

        # type index utils
        self.__nt_index = self.snapshot.nativeTypeIndex
        self.__mt_index = self.snapshot.managedTypeIndex

        # connections
        self.joint_bridges: List[JointBridge] = []
        self.references_from: Dict[int, List[JointBridge]] = {}
        self.references_to: Dict[int, List[JointBridge]] = {}

        self.__bridge_visit:Dict[str, JointBridge] = {}
        self.__type_address_map: Dict[int, int] = {}
        self.__native_object_address_map: Dict[int, int] = {}
        self.__managed_object_address_map: Dict[int, int] = {}
        self.__managed_native_address_map: Dict[int, int] = {}
        self.__handle_address_map: Dict[int, int] = {}

    def crawl(self):
        self.init_managed_types()
        self.init_native_connections()
        self.crawl_handles()
        self.crawl_static()

    def init_managed_types(self):
        managed_types = self.snapshot.typeDescriptions
        for n in range(len(managed_types)):
            mt = managed_types[n]
            mt.isUnityEngineObjectType = self.is_subclass_of_managed_type(mt, self.__mt_index.unityengine_Object)

    def init_native_connections(self, exclude_native: bool = False):
        managed_start = 0
        managed_stop = managed_start + len(self.snapshot.gcHandles)
        native_start = managed_start + managed_stop
        native_stop = native_start + len(self.snapshot.nativeObjects)
        managed_connections = []
        for it in self.snapshot.connections:
            connection = JointBridge(src=it.from_, dst=it.to)
            connection.src_kind = BridgeKind.handle
            if native_start <= connection.src < native_stop:
                connection.src -= native_start
                connection.src_kind = BridgeKind.native
            connection.dst_kind = BridgeKind.handle
            if native_start <= connection.dst < native_stop:
                connection.dst -= native_start
                connection.dst_kind = BridgeKind.native
            if exclude_native and connection.src_kind == BridgeKind.native: continue
            managed_connections.append(connection)
            self.try_accept_connection(connection=connection, from_native=True)
        self.joint_bridges = managed_connections

    def try_accept_connection(self, connection:JointBridge, from_native:bool = False):
        if connection.src >= 0 and not from_native:
            src_object = self.managed_objects[connection.src]
            if src_object.address in self.__bridge_visit:
                dst_object = self.managed_objects[connection.dst]
                if self.__bridge_visit.get(src_object.address) == dst_object.address: return
                self.__bridge_visit[src_object.address] = dst_object.address

        if connection.src_kind != BridgeKind.none and connection.src != -1:
            key = self.get_index_key(kind=connection.src_kind, index=connection.src)
            if key not in self.references_from:
                self.references_from[key] = []
            self.references_from[key].append(connection)
        if connection.dst_kind != BridgeKind.none and connection.dst != -1:
            if connection.dst < 0: connection.dst = -connection.dst
            key = self.get_index_key(kind=connection.dst_kind, index=connection.dst)
            if key not in self.references_to:
                self.references_to[key] = []
            self.references_to[key].append(connection)

    def get_index_key(self, kind: BridgeKind, index: int):
        return (kind.value << 28) + index

    def get_connection_key(self, connection:JointBridge):
        return self.get_index_key(kind=connection.src_kind, index=connection.src) << 32 \
               | self.get_index_key(kind=connection.dst_kind, index=connection.dst)

    def find_type_of_address(self, address: int) -> int:
        type_index = self.find_type_at_type_info_address(type_info_address=address)
        if type_index != -1: return type_index
        type_ptr = self.heap_memory.read_pointer(address)
        if type_ptr == 0: return -1
        vtable_ptr = self.heap_memory.read_pointer(type_ptr)
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

    def get_connections_of(self, kind: BridgeKind, managed_object_index: int) -> List[JointBridge]:
        key = self.get_index_key(kind=kind, index=managed_object_index)
        references = self.references_from.get(key)
        return references if references else []

    def get_connections_referenced_by(self, kind: BridgeKind, managed_object_index: int) -> List[JointBridge]:
        key = self.get_index_key(kind=kind, index=managed_object_index)
        references = self.references_to.get(key)
        return references if references else []

    def get_connections_in_heap_section(self, section: MemorySection) -> List[JointBridge]:
        if not section.bytes: return []
        start_address = section.startAddress
        stop_address = start_address + len(section.bytes)
        references = []
        for n in range(len(self.managed_objects)):
            mo = self.managed_objects[n]
            if start_address <= mo.address < stop_address:
                references.append(JointBridge(dst=n, dst_kind=BridgeKind.managed))
        return references

    def dump_managed_object_reference_chain(self, object_index:int, indent:int = 2, level:int = 2)->str:
        buffer = io.StringIO()
        indent_space = ' ' * indent
        reference_chains = self.__retrieve_reference_chains(object_index=object_index, level=level)
        for chain in reference_chains:
            buffer.write(indent_space)
            self.__format_reference_chain(reference_chain=chain, buffer=buffer, indent=indent + 2)
            buffer.write('\n')
        buffer.seek(0)
        return buffer.read()

    def __retrieve_reference_chains(self, object_index:int, level:int, reference_chain:List[JointBridge] = [], anti_circular = ())->List[List[JointBridge]]:
        chain_array = []  # type: list[list[JointBridge]]
        if object_index == -1:
            if reference_chain: chain_array.append(reference_chain)
        else:
            mo = self.managed_objects[object_index]
            index_key = self.get_index_key(kind=BridgeKind.managed, index=mo.managed_object_index)
            references = self.references_to.get(index_key)
            if references:
                for connection in references[:level]:
                    if mo.address in anti_circular: continue
                    chain_array += self.__retrieve_reference_chains(object_index=connection.src, reference_chain=reference_chain + [connection], anti_circular=anti_circular + (mo.address,), level=level)
        return chain_array

    def __format_reference_chain(self, reference_chain:List[JointBridge], buffer:io.StringIO, indent:int = 4):
        managed_types = self.snapshot.typeDescriptions
        indent_space = '\n' + ' ' * indent + '.'
        top_complete = False
        for n in range(len(reference_chain)):
            connection = reference_chain[-(n+1)]
            joint = connection.joint
            object_type = managed_types[joint.object_type_index]

            if top_complete: buffer.write(indent_space)
            if joint.handle_index >= 0:
                buffer.write('GCHandle::{}'.format(object_type.name))
            else:
                field_type = managed_types[joint.field_type_index]
                object_base_type = object_type
                field = None
                while object_base_type.baseOrElementTypeIndex >= 0:
                    if joint.field_index < len(object_base_type.fields):
                        field = object_base_type.fields[joint.field_index]
                        if field.offset == joint.field_offset: break
                    object_base_type = self.snapshot.typeDescriptions[object_base_type.baseOrElementTypeIndex]
                assert field
                if joint.array_index >= 0:
                    component = '{{{}:{}}}[{}]'.format(field.name, field_type.name, joint.array_index)
                else:
                    component = '{{{}:{}}}'.format(field.name, field_type.name)
                if joint.is_static: component = '{}::{}'.format(object_type.name, component)
                buffer.write(component)
                if joint.field_address > 0: buffer.write(' 0x{:08x}'.format(joint.field_address))
            top_complete = True

    def find_mono_script_type(self, native_index: int) -> Tuple[str, int]:
        key = self.get_index_key(kind=BridgeKind.native, index=native_index)
        reference_list = self.references_from.get(key)
        if reference_list:
            for refer in reference_list:
                type_index = self.snapshot.nativeObjects[refer.dst].nativeTypeArrayIndex
                if refer.dst_kind == BridgeKind.native and type_index == self.__nt_index.MonoScript:
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
        managed_object.is_value_type = mt.isValueType
        if mt.isValueType or mt.isArray or not mt.isUnityEngineObjectType: return
        native_address = self.heap_memory.read_pointer(managed_object.address + self.__cached_ptr.offset)
        if native_address == 0: return
        native_object_index = self.find_native_object_at_address(address=native_address)
        if native_object_index == -1: return
        # connect native object and managed object
        native_type_index = self.snapshot.nativeObjects[native_object_index].nativeTypeArrayIndex
        managed_object.native_object_index = native_object_index
        managed_object.native_size = self.snapshot.nativeObjects[native_object_index].size
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
        self.managed_objects.append(mo)
        return mo

    def is_crawlable(self, type:TypeDescription)->bool:
        return not type.isValueType or type.size > 8 # self.vm.pointerSize

    def crawl_managed_array_address(self, address:int, type:TypeDescription, memory_reader:HeapReader, joint:ActiveJoint, depth:int):
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
            self.crawl_managed_entry_address(address=element_address, type=element_type, memory_reader=memory_reader,
                                             joint=joint.clone(array_index=n), depth=depth+1)

    def crawl_managed_entry_address(self, address:int, type:TypeDescription, memory_reader:HeapReader, joint:ActiveJoint, is_real_type:bool = False, depth = 0):
        is_static_crawling = isinstance(memory_reader, StaticFieldReader)
        if address < 0 or not is_static_crawling and address == 0: return
        if depth >= 512:
            print('!![ITER_MAX={}] address={:08x} type={}'.format(depth, address, type))
            return
        if type and type.isValueType:
            type_index = type.typeIndex
        else:
            if is_real_type:
                type_index = type.typeIndex
            else:
                type_index = self.find_type_of_address(address=address)
                if type_index == -1:
                    type_index = type.typeIndex if type else -1
        if type_index == -1: return
        entry_type = self.snapshot.typeDescriptions[type_index]
        if entry_type.isValueType or address not in self.__visit:
            mo = self.create_managed_object(address=address, type_index=type_index)
            mo.handle_index = joint.handle_index
            mo.joint = joint
            assert mo and mo.managed_object_index >= 0, mo
            self.set_object_size(managed_object=mo, type=self.snapshot.typeDescriptions[type_index], memory_reader=memory_reader)
            # print(mo)
        else:
            object_index = self.__visit.get(address)
            mo = self.managed_objects[object_index]
            assert mo
        if joint.handle_index >= 0:
            connection = JointBridge(src_kind=BridgeKind.handle, src=joint.object_index,
                                     dst_kind=BridgeKind.managed, dst=mo.managed_object_index, joint=joint)
        else:
            if joint.is_static:
                connection = JointBridge(src_kind=BridgeKind.static, src=-1,
                                         dst_kind=BridgeKind.managed, dst=mo.managed_object_index, joint=joint)
            else:
                connection = JointBridge(src_kind=BridgeKind.managed, src=joint.object_index,
                                         dst_kind=BridgeKind.managed, dst=mo.managed_object_index, joint=joint)
        self.try_accept_connection(connection=connection)
        if not entry_type.isValueType:
            if address in self.__visit: return
            self.__visit[mo.address] = mo.managed_object_index

        if entry_type.isArray: # crawl array
            self.crawl_managed_array_address(address=address, type=entry_type, memory_reader=memory_reader, joint=joint, depth=depth+1)
            return
        mother_joint = ActiveJoint(object_type_index=type_index, object_index=mo.managed_object_index, object_address=mo.address)
        dive_type = entry_type
        while dive_type:
            for field in dive_type.fields: # crawl fields
                field_type = self.snapshot.typeDescriptions[field.typeIndex]
                if field.isStatic: continue
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
                    field_type_index = self.find_type_of_address(address=field_address)
                    if field_type_index != -1: field_type = self.snapshot.typeDescriptions[field_type_index]
                pass_memory_reader = memory_reader if field_type.isValueType else self.heap_memory
                self.crawl_managed_entry_address(address=field_address, type=field_type, memory_reader=pass_memory_reader, is_real_type=True,
                                                 joint=mother_joint.clone(field_type_index=field_type.typeIndex, field_index=field.fieldSlotIndex, field_offset=field.offset, field_address=field_address),
                                                 depth=depth + 1)
            dive_type = self.snapshot.typeDescriptions[dive_type.baseOrElementTypeIndex] if dive_type.baseOrElementTypeIndex >= 0 else None

    def crawl_handles(self):
        for item in self.snapshot.gcHandles:
            self.crawl_managed_entry_address(address=item.target, joint=ActiveJoint(handle_index=item.gcHandleArrayIndex),
                                             memory_reader=self.heap_memory, type=None)

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
                    memory_reader = self.heap_memory
                self.crawl_managed_entry_address(address=field_address, type=field_type, memory_reader=memory_reader,
                                                 joint=ActiveJoint(object_type_index=mt.typeIndex, field_index=field.fieldSlotIndex, field_type_index=field.typeIndex, is_static=True))





























