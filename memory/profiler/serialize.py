from .stream import MemoryStream
from .core import *

class MemorySnapshotReader(object):
    def __init__(self, debug:bool = True):
        self.stream = MemoryStream()
        self.debug = debug

    def read(self, file_path): # type: (str)->PackedMemorySnapshot
        self.stream.open(file_path)
        return self.readObject(input=self.stream)

    def readObject(self, input:MemoryStream):
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
                    field_data = self.readArray(input)
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
                field_data = self.readObject(input)
                assert nest_class_name == field_data.__class__.__name__, '++ expect={} but={}'.format(nest_class_name, field_data.__class__.__name__)
            setattr(data, field_name, field_data)
        return data

    def readArray(self, input:MemoryStream):
        item_list = []
        count = input.read_uint32()
        for n in range(count):
            item_list.append(self.readObject(input))
        return item_list


