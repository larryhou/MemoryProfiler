#!/usr/bin/env python3

import time, io, math, struct, os
import os.path as p


class TimeSampler(object):
    def __init__(self, name='Summary', workspace='__perf'):
        self.__name = name
        self.__workspace = workspace
        if not p.exists(self.__workspace):
            os.makedirs(self.__workspace)
        self.__sequence = 0
        self.__event_map = {}  # type: dict[int, tuple[str, float]]
        self.__bridge = {}  # type: dict[int, int]
        self.__cursor = []  # type: list[int]
        self.__record = {}  # type: dict[int,tuple[str, float]]
        self.__index_formatter = '{:2}'
        self.__entities = [] # type: list[int]
        self.begin(event=self.__name)

    @property
    def name(self): return self.__name
    @name.setter
    def name(self, v):
        self.__name = v
        summary_index = 0
        if summary_index in self.__event_map:
            _, timestamp = self.__event_map[summary_index]
            self.__event_map[summary_index] = self.__name, timestamp
        if summary_index in self.__record:
            _, elapse = self.__record[summary_index]
            self.__record[summary_index] = self.__name, elapse

    def reset(self):
        self.__init__()

    def begin(self, event):  # type: (str)->int
        sequence = self.__sequence
        self.__sequence += 1
        self.__event_map[sequence] = event, time.process_time()
        if self.__cursor:
            self.__bridge[sequence] = self.__cursor[-1]
        else:
            self.__entities.append(sequence)
        self.__cursor.append(sequence)
        return sequence

    def end(self):  # type: ()->float
        if not self.__cursor: return 0.0
        id = self.__cursor[-1]
        event, timestamp = self.__event_map[id]
        event_elapse = time.process_time() - timestamp
        self.__record[id] = event, event_elapse
        del self.__event_map[id]
        del self.__cursor[-1]
        return event_elapse

    def done(self):
        if len(self.__cursor) == 1 and self.__cursor[0] == 0:
            self.end()

    def save(self): # type: ()->str
        self.done()
        assert not self.__event_map, self.__event_map
        if not self.__record: return
        bridge_map = {}
        for child, parent in self.__bridge.items():
            if parent not in bridge_map:
                bridge_map[parent] = []
            bridge_map[parent].append(child)
        entity_count = len(self.__entities)
        buffer = io.BytesIO()
        buffer.write(b'PERF')
        buffer.write(struct.pack('>I', len(self.__record)))
        buffer.write(struct.pack('>I', entity_count))
        for n in range(entity_count):
            entity_index = self.__entities[n]
            self.__encode_entity(index=entity_index, bridge_map=bridge_map, buffer=buffer)
        buffer.seek(0)
        with open('{}/{}.hex'.format(self.__workspace, self.name), 'wb') as fp:
            fp.write(buffer.read())
            return fp.name

    def dump(self, file_path): # type: (str)->str
        buffer = io.StringIO()
        with open(file_path, 'rb') as fp:
            assert fp.read(4) == b'PERF'
            record_count, = struct.unpack('>I', fp.read(4))
            entity_count, = struct.unpack('>I', fp.read(4))
            self.__index_formatter = self.__get_index_formatter(entity_count=record_count)
            for _ in range(entity_count):
                self.__read(fp=fp, buffer=buffer, indent='')
        buffer.seek(0)
        return buffer.read()

    def __read(self, fp, buffer, indent):
        index, = struct.unpack('>I', fp.read(4))
        count, = struct.unpack('>I', fp.read(4))
        event = fp.read(count).decode('utf-8')
        elapse, = struct.unpack('>d', fp.read(8))
        buffer.write(indent)
        buffer.write('[{}] {}={:.6f}\n'.format(self.__index_formatter.format(index), event, elapse))
        child_count, = struct.unpack('>I', fp.read(4))
        for _ in range(child_count):
            self.__read(fp=fp, indent=indent + '    ', buffer=buffer)

    def __encode_entity(self, index, bridge_map, buffer:io.BytesIO):
        event, elapse = self.__record[index]  # type: str, float
        buffer.write(struct.pack('>I', index))
        event_data = event.encode('utf-8')
        buffer.write(struct.pack('>I', len(event_data)))
        buffer.write(event_data)
        buffer.write(struct.pack('>d', elapse))
        if index in bridge_map:
            children = bridge_map[index]
            buffer.write(struct.pack('>I', len(children)))
            for child_index in children:
                self.__encode_entity(index=child_index, bridge_map=bridge_map, buffer=buffer)
        else:
            buffer.write(struct.pack('>I', 0))

    def __get_index_formatter(self, entity_count):
        digit_count = int(math.ceil(math.log(entity_count, 10)))
        return '{:0%dd}' % digit_count

    def summary(self):
        self.done()
        assert not self.__event_map, self.__event_map
        if not self.__record: return
        self.__index_formatter = self.__get_index_formatter(entity_count=len(self.__record))
        bridge_map = {}
        for child, parent in self.__bridge.items():
            if parent not in bridge_map:
                bridge_map[parent] = []
            bridge_map[parent].append(child)
        buffer = io.StringIO()
        for entity in self.__entities:
            self.__write(bridge_map, index=entity, buffer=buffer)
        buffer.seek(0)
        print(buffer.read())
        self.save()

    def __write(self, bridge_map, index, buffer,
                indent=''):  # type: (dict[int, list[int]], int, io.StringIO, str)->io.StringIO
        event, elapse = self.__record.get(index)
        buffer.write(indent)
        buffer.write('[{}] {}={:.6f}\n'.format(self.__index_formatter.format(index), event, elapse))
        if index in bridge_map:
            for child in bridge_map.get(index):
                self.__write(bridge_map, index=child, buffer=buffer, indent=indent + '    ')
        return buffer

if __name__ == '__main__':
    ts = TimeSampler()
    ts.begin('a')
    ts.begin('b')
    ts.end()
    ts.begin('c')
    ts.end()
    ts.end()
    ts.summary()
