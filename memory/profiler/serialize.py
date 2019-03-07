import sys
from .core import *
from .stream import MemoryStream

class MemorySnapshot(object):
    def __init__(self):
        self.stream = MemoryStream()

    def load(self, file_path:str):
        self.stream.open(file_path)
        self.readObject(input=self.stream)

    def readObject(self, input:MemoryStream):
        class_type = input.read_utfstring() # type: str
        class_name = class_type.split('.')[-1]
        print('+ readObject offset={} class_type={}'.format(input.position, class_type))
        data = globals().get(class_name)() # type: object
        assert data
        field_count = input.read_ubyte()
        print(field_count, input.position)
        for n in range(field_count):
            field_name = input.read_utfstring()
            field_type = input.read_utfstring()
            print(field_name, field_type)
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


