from .core import PackedMemorySnapshot, TypeDescription
from struct import unpack
import operator

class Quaternion(object):
    def __init__(self, x = 0.0, y = 0.0, z = 0.0, w = 0.0):
        self.x = x
        self.y = y
        self.z = z
        self.w = w

    def __repr__(self):
        return '[Quaternion] x={} y={} z={} w={}'.format(self.x, self.y, self.z, self.w)

class Decimal(object):
    def __init__(self, flags = 0, hi = 0, lo = 0, mid = 0):
        self.flags = flags
        self.hi = hi
        self.lo = lo
        self.mid = mid

    def __repr__(self):
        return '[Decimal] flags={} hi={} lo={} mid={}'.format(self.flags, self.hi, self.lo, self.mid)

class HeapSegment(object):
    def __init__(self, memory:bytes, offset:int, length:int):
        self.memory = memory
        self.offset = offset
        self.length = length
        self.qualified = length <= len(memory) - offset

    def __repr__(self):
        return '[HeapSegment] memory={:,} offset={} length={:,} qualified={}'.format(
            len(self.memory), self.offset, self.length, self.qualified
        )

class HeepReader(object):
    def __init__(self, snapshot:PackedMemorySnapshot):
        self.snapshot = snapshot
        self.heep_sections = snapshot.managedHeapSections
        self.heep_sections.sort(key=operator.attrgetter('startAddress'))

        self.memory:bytes = bytes()
        self.start_address:int = -1
        self.stop_address:int = -1

    def try_begin_read(self, address:int)->int:
        if address == 0: return -1
        if self.start_address <= address < self.stop_address:
            return address - self.start_address

        heap_index = self.find_heap_of_address(address=address)
        if heap_index == -1: return -1

        heap = self.heep_sections[heap_index]
        self.start_address = heap.startAddress
        self.stop_address = self.start_address + (len(heap.bytes) if heap.bytes else 0)
        self.memory = heap.bytes
        return address - self.start_address

    def read_sbyte(self, address:int)->int:
        offset = self.try_begin_read(address)
        if offset == -1: return 0
        return unpack('b', self.memory[offset:offset+1])[0]

    def read_ubyte(self, address:int)->int:
        offset = self.try_begin_read(address)
        if offset == -1: return 0
        return self.memory[offset]

    def read_char(self, address:int)->str:
        offset = self.try_begin_read(address)
        if offset == -1: return ''
        return chr(self.memory[offset])

    def read_boolean(self, address:int)->bool:
        return self.read_ubyte(address) != 0

    def read_uint8(self, address:int)->int:
        return self.read_ubyte(address)

    def read_sint8(self, address:int)->int:
        return self.read_sbyte(address)

    def read_uint16(self, address:int)->int:
        offset = self.try_begin_read(address)
        if offset == -1: return 0
        return unpack('>H', self.memory[offset:offset+2])[0]

    def read_sint16(self, address:int)->int:
        offset = self.try_begin_read(address)
        if offset == -1: return 0
        return unpack('>h', self.memory[offset:offset+2])[0]

    def read_uint32(self, address:int)->int:
        offset = self.try_begin_read(address)
        if offset == -1: return 0
        return unpack('>I', self.memory[offset:offset+4])[0]

    def read_sint32(self, address:int)->int:
        offset = self.try_begin_read(address)
        if offset == -1: return 0
        return unpack('>i', self.memory[offset:offset+4])[0]

    def read_uint64(self, address:int)->int:
        offset = self.try_begin_read(address)
        if offset == -1: return 0
        return unpack('>Q', self.memory[offset:offset+8])[0]

    def read_sint64(self, address:int)->int:
        offset = self.try_begin_read(address)
        if offset == -1: return 0
        return unpack('>q', self.memory[offset:offset+8])[0]

    def read_single(self, address:int)->int:
        offset = self.try_begin_read(address)
        if offset == -1: return 0.0
        return unpack('>f', self.memory[offset:offset+4])[0]

    def read_double(self, address:int)->int:
        offset = self.try_begin_read(address)
        if offset == -1: return 0.0
        return unpack('>d', self.memory[offset:offset+8])[0]

    def read_decimal(self, address:int)->int:
        offset = self.try_begin_read(address)
        if offset == -1: return Decimal()
        return Decimal(
            flags=self.read_uint32(address + 0),
            hi=self.read_uint32(address + 4),
            lo=self.read_uint32(address + 8),
            mid=self.read_uint32(address + 12),
        )

    def read_quaternion(self, address:int)->Quaternion:
        offset = self.try_begin_read(address)
        if offset == -1: return Quaternion()
        type_size = 4
        return Quaternion(
            x=self.read_single(address=address + type_size * 0),
            y=self.read_single(address=address + type_size * 1),
            z=self.read_single(address=address + type_size * 2),
            w=self.read_single(address=address + type_size * 3)
        )

    def read_matrix4x4(self, address:int):
        offset = self.try_begin_read(address)
        if offset == -1: return (0,)*16
        return tuple([self.read_single(address + 4 * n) for n in range(16)])

    def read_pointer(self, address:int)->int:
        return self.read_uint64(address) if self.snapshot.vm.pointerSize == 8 else self.read_uint32(address)

    def read_string(self, address:int)->str:
        offset = self.try_begin_read(address)
        if offset == -1: return ''
        length = self.read_sint32(address)
        if length <= 0:
            return 'length[{}] less then 0'.format(length) if length < 0 else ''
        offset += 4 # string length
        length *= 2 # wide char
        return self.memory[offset:offset+length].decode(encoding='utf-16') # unicode encoding

    def read_string_length(self, address:int)->int:
        offset = self.try_begin_read(address)
        if offset == -1: return 0
        length = self.read_sint32(address)
        return length if length > 0 else 0

    def read_array_length(self, address:int, array_type:TypeDescription)->int:
        vm = self.snapshot.vm
        bounds = self.read_pointer(address + vm.arrayBoundsOffsetInHeader)
        if bounds == 0:
            return self.read_pointer(address + vm.arraySizeOffsetInHeader)
        length = 1
        for n in range(array_type.arrayRank):
            length *= self.read_pointer(bounds + n * vm.pointerSize)
        return length

    def read_array_length_of_dimension(self, address:int, array_type:TypeDescription, dimension:int)->int:
        if dimension >= array_type.arrayRank: return 0
        vm = self.snapshot.vm
        bounds = self.read_pointer(address + vm.arrayBoundsOffsetInHeader)
        if bounds == 0:
            return self.read_pointer(address + vm.arraySizeOffsetInHeader)
        return self.read_pointer(bounds + dimension * vm.pointerSize)

    def read_object_size(self, address:int, object_type:TypeDescription)->int:
        offset = self.try_begin_read(address)
        if offset == -1: return 0
        if object_type.isArray:
            if object_type.baseOrElementTypeIndex < 0 or object_type.baseOrElementTypeIndex >= len(self.snapshot.typeDescriptions):
                print('[ERR] not invalid baseOrElementTypeIndex={0}'.format(object_type.baseOrElementTypeIndex))
                return 0
            array_length = self.read_array_length(address, array_type=object_type)
            element_type = self.snapshot.typeDescriptions[object_type.baseOrElementTypeIndex]
            element_size = element_type.size if element_type.isValueType else self.snapshot.vm.pointerSize
            return self.snapshot.vm.arrayHeaderSize + element_size * array_length
        if object_type.name == 'System.String':
            size = self.snapshot.vm.objectHeaderSize
            size += 4 # string length
            size += self.read_string_length(address + self.snapshot.vm.objectHeaderSize) * 2 # string data size
            size += 2 # \x00\x00
            return size
        return object_type.size

    def read_object_memory(self, address:int, object_type:TypeDescription)->HeapSegment:
        size = self.read_object_size(address, object_type)
        if size <= 0:
            return HeapSegment(b'', 0, 0)
        offset = self.try_begin_read(address)
        if offset < 0:
            return HeapSegment(b'', 0, 0)
        return HeapSegment(self.memory, offset, size)

    def find_heap_of_address(self, address)->int:
        idx_min, idx_max = 0, len(self.heep_sections) - 1
        while idx_min <= idx_max:
            idx_mid = (idx_max + idx_min) >> 1
            heep = self.heep_sections[idx_mid]
            if heep.startAddress > address:
                idx_max = idx_mid - 1
            elif heep.startAddress + len(heep.bytes) < address:
                idx_min = idx_mid + 1
            else:
                return idx_mid
        return -1

class StaticHeapReader(HeepReader):

    def __init__(self, snapshot: PackedMemorySnapshot, memory: bytes):
        super().__init__(snapshot)
        self.load_memory(memory)

    def load_memory(self, memory:bytes):
        self.memory = memory
        self.start_address = 0
        self.stop_address = len(memory)

    def try_begin_read(self, address:int):
        if address == 0 \
            or len(self.memory) == 0 or address >= len(self.memory):
            return -1
        return address


