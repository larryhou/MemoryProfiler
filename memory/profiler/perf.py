#!/usr/bin/env python3

import time, io, math


class TimeSampler(object):
    def __init__(self, name='Summary'):
        self.__sequence = 0
        self.__event_map = {}  # type: dict[int, str]
        self.__bridge = {}  # type: dict[int, int]
        self.__cursor = []  # type: list[int]
        self.__record = {}  # type: dict[int, float]
        self.__index_formatter = '{:2}'
        self.__entities = [] # type: list[int]
        self.begin(name)

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

    def finish(self):
        if len(self.__cursor) == 1 and self.__cursor[0] == 0:
            self.end()

    def summary(self):
        self.finish()
        assert not self.__event_map, self.__event_map
        if not self.__bridge: return
        digit_count = int(math.ceil(math.log(len(self.__bridge), 10)))
        self.__index_formatter = '{:0%dd}' % digit_count
        bridge_tree = {}
        for child, parent in self.__bridge.items():
            if parent not in bridge_tree:
                bridge_tree[parent] = []
            bridge_tree[parent].append(child)
        for entity in self.__entities:
            buffer = self.__dump(bridge_tree, index=entity)
            buffer.seek(0)
            print(buffer.read())

    def __dump(self, bridge_tree, index, buffer=None,
               indent=''):  # type: (dict[int, list[int]], int, io.StringIO, str)->io.StringIO
        if not buffer: buffer = io.StringIO()
        event, elapse = self.__record.get(index)
        buffer.write(indent)
        buffer.write('[{}] {}={:.6f}\n'.format(self.__index_formatter.format(index), event, elapse))
        if index in bridge_tree:
            for child in bridge_tree.get(index):
                self.__dump(bridge_tree, index=child, buffer=buffer, indent=indent + '    ')
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
