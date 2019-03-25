from .stream import MemoryStream
from .core import *
import time, uuid

class NativeMemoryRef(object):
    def __init__(self, address:int, stream:MemoryStream, offset:int, length:int):
        self.__stream = stream
        self.__offset = offset
        self.__length = length
        self.__address = address

    @property
    def address(self): return self.address

    def read(self)->bytes:
        position = self.__stream.position
        self.__stream.position = self.__offset
        memory = self.__stream.read(self.__length)
        self.__stream.position = position
        return memory

    def __repr__(self):
        return '[NativeMemoryRef] address={:x} offset={} length={}'.format(self.__address, self.__offset, self.__length)

class MemorySnapshotReader(object):
    def __init__(self, debug:bool = True):
        self.__stream = MemoryStream()
        self.vm:VirtualMachineInformation = None
        self.debug = debug
        self.cached_ptr:FieldDescription = None

        self.mime:str = 'PMS'
        self.unity_version:str = ''
        self.description:str = ''
        self.system_version:str = ''
        self.create_time:str = ''
        self.total_size:int = 0
        self.uuid:str = ''

        self.native_memory_map = {}  # type: dict[int, NativeMemoryRef]
        self.snapshot:PackedMemorySnapshot = None

    def __read_header(self, input:MemoryStream):
        self.mime = input.read(size=3).decode('ascii')
        self.description = input.read_utfstring()
        self.unity_version = input.read_utfstring()
        self.system_version = input.read_utfstring()
        self.uuid = uuid.UUID(bytes=input.read(16))
        self.total_size = input.read_uint32()
        self.create_time = self.__read_timestamp(input=input)
        print('[MemorySnapshotReader] mime={!r} unity_version={!r} system_version={!r} create_time={!r} total_size={:,} uuid={!r}'.format(
            self.mime, self.unity_version, self.system_version, self.create_time, self.total_size, str(self.uuid)
        ))

    def __read_timestamp(self, input:MemoryStream):
        time_scale = 10**6
        time_value = input.read_uint64()
        seconds = float(time_value) / time_scale
        fraction = time_value % time_scale
        return time.strftime('%Y-%m-%dT%H:%M:%S.{:06d}'.format(fraction), time.localtime(seconds))

    def __read_native_memory(self, input:MemoryStream):
        native_count = input.read_uint32()
        size_limit = 1 << 25
        self.native_memory_map = {}
        for n in range(native_count):
            offset = input.position
            address = input.read_uint64()
            length = input.read_uint32()
            if 0 < length <= size_limit:
                ref = self.native_memory_map[address] = NativeMemoryRef(address=address, stream=input, offset=offset, length=length)
                input.position += length
                if self.debug: print(ref)
            else:
                assert input.read_uint32() == 0, 'address={:x} offset={} length={} {}/{}'.format(address, offset, length, n+1, native_count)

    def __read_snapshot(self, input:MemoryStream):
        self.vm = self.__read_object(input=input)
        print(self.vm)
        snapshot = self.__read_object(input=input)  # type: PackedMemorySnapshot
        snapshot.initialize()
        assert snapshot.cached_ptr
        self.cached_ptr = snapshot.cached_ptr
        return snapshot

    def read(self, file_path): # type: (str)->PackedMemorySnapshot
        stream = self.__stream.open(file_path)
        self.__read_header(input=self.__stream)
        self.snapshot = None
        while stream.bytes_available > 0:
            offset = stream.position
            block_length = stream.read_uint32()
            block_type = stream.read(1)
            if block_type == b'0':
                self.snapshot = self.__read_snapshot(input=stream)
                assert stream.position == offset + block_length, 'stream.position expect={} but={}'.format(offset + block_length, stream.position)
            elif block_type == b'1':
                self.__read_native_memory(input=stream)
                assert stream.position == offset + block_length, 'stream.position expect={} but={}'.format(offset + block_length, stream.position)
            else:
                self.__stream.position += block_length - 5
            timestamp = self.__read_timestamp(input=stream)
            self.debug: print(timestamp)
        return self.snapshot

    def __read_object(self, input:MemoryStream):
        class_type = input.read_utfstring() # type: str
        class_name = class_type.split('.')[-1]
        if self.debug: print('+ readObject offset={} class_type={}'.format(input.position, class_type))
        data = globals().get(class_name)() # type: object
        assert data
        field_count = input.read_ubyte()
        if self.debug: print(field_count, input.position)
        for n in range(field_count):
            field_name = input.read_utfstring()
            if field_name == 'from': field_name = 'from_'
            field_type = input.read_utfstring()
            if self.debug: print('    - {} {}'.format(field_name, field_type))
            if field_type.endswith('[]'):
                if field_type.endswith('Byte[]'):
                    field_data = input.read(size=input.read_uint32())
                else:
                    field_data = self.__read_array(input)
            elif field_type.endswith('Int32'):
                field_data = input.read_sint32()
            elif field_type.endswith('UInt32'):
                field_data = input.read_uint32()
            elif field_type.endswith('Int64'):
                field_data = input.read_sint64()
            elif field_type.endswith('UInt64'):
                field_data = input.read_uint64()
            elif field_type.endswith('String'):
                field_data = input.read_utfstring()
            elif field_type.endswith('Boolean'):
                field_data = input.read_ubyte() != 0
            elif field_type.endswith('Flags'):
                field_data = input.read_uint32()
            else:
                nest_class_name = field_type.split('.')[-1]
                field_data = self.__read_object(input)
                assert nest_class_name == field_data.__class__.__name__, '++ expect={} but={}'.format(nest_class_name, field_data.__class__.__name__)
            setattr(data, field_name, field_data)
        setattr(data, 'vm', self.vm)
        return data

    def __read_array(self, input:MemoryStream):
        item_list = []
        count = input.read_uint32()
        for n in range(count):
            item_list.append(self.__read_object(input))
        return item_list


