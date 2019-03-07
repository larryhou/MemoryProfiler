#!/usr/bin/env python3
#encoding:utf-8

import io, os, struct, binascii

UINT32_MAX_VALUE = 0xFFFFFFFF

class TestCase(object):
    def run_test_suit(self):
        self.test_sqlit_sint32()
        self.test_compact_sint32()

    def test_sqlit_sint32(self):
        import random
        value_list = []
        stream = MemoryStream('>')
        for _ in range(100000):
            value = -random.randint(0, 0x7FFFFFFF)
            value_list.append(value)
            stream.write_sqlit_sint32(value)
        stream.position = 0
        for n in range(len(value_list)):
            assert stream.read_sqlit_sint32() == value_list[n]
        assert stream.position == stream.length

    def test_compact_sint32(self):
        import random
        value_list = []
        stream = MemoryStream('>')
        for _ in range(100000):
            value = -random.randint(0, 0x7FFFFFFF)
            value_list.append(value)
            stream.write_compact_sint32(value)
        stream.position = 0
        for n in range(len(value_list)):
            assert stream.read_compact_sint32() == value_list[n]
        assert stream.position == stream.length
    
def run_test():
    TestCase().run_test_suit()

class MemoryStream(object):
    LITTLE_ENDIAN = '<'
    BIG_ENDIAN = '>'
    def __init__(self, endian = '>'):
        self.endian = MemoryStream.BIG_ENDIAN if endian is None else endian
        self.data = io.BytesIO(bytearray())
        self.path = None

    @property
    def data(self):
        return self.__data
    @data.setter
    def data(self, data):
        self.__data = data
        if hasattr(data, 'name'):
            self.path = os.path.abspath(self.data.name)
        self.position = 0
    
    def open(self, file_path):
        self.data = open(file_path, 'r+b')

    def save(self, output_path = None):
        if output_path is not None:
            output_path = os.path.abspath(output_path)
            if output_path != self.path:
                f = open(output_path, 'wb')
                position = self.position
                self.position = 0
                f.write(self.data.read())
                f.close()
                self.position = position
        self.data.flush()

    def close(self):
        self.data.flush()
        self.data.close()

    @property
    def bytes_available(self):
        return self.length - self.position
    
    @property
    def position(self):
        return self.data.tell()
    @position.setter
    def position(self, position):
        self.data.seek(position, os.SEEK_SET)
    
    @property
    def length(self):
        position = self.position
        self.data.seek(0, os.SEEK_END)
        length = self.data.tell()
        self.data.seek(position, os.SEEK_SET)
        return length
    @length.setter
    def length(self, length):
        position = self.position
        self.data.truncate(length) # r+b
        if length < position:
            self.data.seek(length, os.SEEK_SET)

    def align(self, pad = 4, offset = 0):
        position = self.data.tell() + offset
        new_position = (position + pad - 1) // pad * pad # integer division
        if position != new_position:
            self.data.seek(new_position - offset, os.SEEK_SET)

    # read
    def read_boolean(self):
        return struct.unpack('?', self.data.read(1))[0]

    def read_char(self):
        return self.data.read(1)

    def read_sbyte(self):
        return struct.unpack('b', self.data.read(1))[0]
    def read_ubyte(self):
        return struct.unpack('B', self.data.read(1))[0]

    def read_sint16(self):
        return struct.unpack('{}h'.format(self.endian), self.data.read(2))[0]
    def read_uint16(self):
        return struct.unpack('{}H'.format(self.endian), self.data.read(2))[0]

    def read_sint32(self):
        return struct.unpack('{}i'.format(self.endian), self.data.read(4))[0]
    def read_uint32(self):
        return struct.unpack('{}I'.format(self.endian), self.data.read(4))[0]

    def read_sint64(self):
        return struct.unpack('{}q'.format(self.endian), self.data.read(8))[0]
    def read_uint64(self):
        return struct.unpack('{}Q'.format(self.endian), self.data.read(8))[0]

    def read_float(self):
        return struct.unpack('{}f'.format(self.endian), self.data.read(4))[0]
    def read_double(self):
        return struct.unpack('{}d'.format(self.endian), self.data.read(8))[0]
    
    def read_byte_tuple(self, size = None):
        if size is not None:
            return list(struct.unpack('{}{}B'.format(self.endian, size), self.data.read(size)))
        else:
            string = self.data.read()
            return list(struct.unpack('{}{}B'.format(self.endian, len(string)), string))

    def read(self, size = None):
        if size is None:
            return self.data.read()
        return self.data.read(size)

    def read_hex(self, size = None):
        data = self.read(size)
        return binascii.hexlify(data).decode('ascii')

    def read_sqlit_sint32(self):
        data = struct.pack('>I', self.read_sqlit_uint32())
        return struct.unpack('>i', data)[0]

    def read_sqlit_uint32(self):
        byte0 = self.read_ubyte()
        if byte0 < 241: return byte0
        byte1 = self.read_ubyte()
        if byte0 < 249:
            return 240 + 256 * (byte0 - 241) + byte1
        byte2 = self.read_ubyte()
        if byte0 == 249:
            return 2288 + 256 * byte1 + byte2
        byte3 = self.read_ubyte()
        if byte0 == 250:
            return byte1 << 0 | byte2 << 8 | byte3 << 16
        byte4 = self.read_ubyte()
        if byte0 >= 251:
            return byte1 << 0 | byte2 << 8 | byte3 << 16 | byte4 << 24
        # assert value <= UINT32_MAX_VALUE
        # return value

    def read_compact_sint32(self):
        data = struct.pack('>I', self.read_compact_uint32())
        return struct.unpack('>i', data)[0]

    def read_compact_uint32(self):
        value, shift = 0, 0
        while True:
            byte = self.read_ubyte()
            value |= (byte & 0x7F) << shift
            if byte & 0x80 == 0: return value
            shift += 7
        assert value <= UINT32_MAX_VALUE
        return value

    def read_utfstring(self):
        return self.read_string(size=self.read_uint32())
    
    def read_string(self, size = None, encoding = 'utf-8'): # type: (int, str)->str
        if size is None:
            string:bytes = b''
            while True:
                char = self.data.read(1)
                if char == b'\x00':
                    break
                string += char
        else:
            string:bytes = self.data.read(size)
        if encoding is None:
            return string
        else:
            if string is not None:
                return string.decode(encoding=encoding)
            return None

    def write_string(self, string, encoding = 'utf-8'): # type: (str, str)->None
        bytes = string.encode(encoding = encoding)
        self.__extend_write(bytes)
        self.__extend_write(b'\x00')

    # write
    def write(self, bytes): # type: (bytes)->None
        self.__extend_write(bytes)
    def __extend_write(self, bytes): # type: (bytes)->None
        capacity = self.length - self.position
        if capacity < len(bytes):
            self.length += len(bytes) - capacity
        self.data.write(bytes)

    def write_boolean(self, value):
        data = struct.pack('?', value)
        self.__extend_write(data)

    def write_char(self, value):
        self.__extend_write(value)

    def write_sbyte(self, value):
        data = struct.pack('b', value)
        self.__extend_write(data)
    def write_ubyte(self, value):
        data = struct.pack('B', value)
        self.__extend_write(data)

    def write_sint16(self, value):
        data = struct.pack('{}h'.format(self.endian), value)
        self.__extend_write(data)
    def write_uint16(self, value):
        data = struct.pack('{}H'.format(self.endian), value)
        self.__extend_write(data)

    def write_sint32(self, value):
        data = struct.pack('{}i'.format(self.endian), value)
        self.__extend_write(data)
    def write_uint32(self, value):
        data = struct.pack('{}I'.format(self.endian), value)
        self.__extend_write(data)

    def write_sint64(self, value):
        data = struct.pack('{}q'.format(self.endian), value)
        self.__extend_write(data)
    def write_uint64(self, value):
        data = struct.pack('{}Q'.format(self.endian), value)
        self.__extend_write(data)

    def write_float(self, value):
        data = struct.pack('{}f'.format(self.endian), value)
        self.__extend_write(data)
    def write_double(self, value):
        data = struct.pack('{}d'.format(self.endian), value)
        self.__extend_write(data)

    def write_byte_tuple(self, tuple):
        num = len(tuple) # type(value) = list or tuple
        print('{}{}B'.format(self.endian, num))
        data = struct.pack('{}{}B'.format(self.endian, num), *tuple)
        self.__extend_write(data)

    def write_hex(self, value):
        self.__extend_write(binascii.unhexlify(value))

    def write_sqlit_sint32(self, value):
        self.write_sqlit_uint32(value & UINT32_MAX_VALUE)

    def write_sqlit_uint32(self, value):
        assert value <= UINT32_MAX_VALUE
        if value <= 240:
            self.write_ubyte(value)
            return
        if value <= 2287:
            self.write_ubyte((value - 240) / 256 + 241)
            self.write_ubyte((value - 240) % 256)
            return
        if value <= 67823:
            self.write_ubyte(249)
            self.write_ubyte((value - 2288) / 256)
            self.write_ubyte((value - 2288) % 256)
            return
        if value <= 16777215:
            self.write_ubyte(250)
            self.write_ubyte(value >>  0 & 0xFF)
            self.write_ubyte(value >>  8 & 0xFF)
            self.write_ubyte(value >> 16 & 0xFF)
            return
        self.write_ubyte(251)
        self.write_ubyte(value >>  0 & 0xFF)
        self.write_ubyte(value >>  8 & 0xFF)
        self.write_ubyte(value >> 16 & 0xFF)
        self.write_ubyte(value >> 24 & 0xFF)

    def write_compact_sint32(self, value):
        self.write_compact_uint32(value & UINT32_MAX_VALUE)

    def write_compact_uint32(self, value):
        assert value <= UINT32_MAX_VALUE
        while value > 0:
            byte = value & 0x7F
            value >>= 7
            if value > 0: byte |= (1 << 7)
            self.write_ubyte(byte)
