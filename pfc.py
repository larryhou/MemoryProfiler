#!/usr/bin/env python3
import os
from struct import unpack

class StackSample(object):
    def __init__(self, id = 0, name = '', calls_count = 0, alloc_bytes = 0, total_time = 0.0, time = 0.0):
        self.id = id
        self.name = name
        self.calls_count = calls_count
        self.alloc_bytes = alloc_bytes
        self.total_time = total_time
        self.time = time

    def __repr__(self):
        return 'id={} name={} calls_count={} alloc_bytes={} total_time={:.9} time={:.9f}'.format(
            self.id, self.name, self.calls_count, self.alloc_bytes, self.total_time, self.time
        )

def reveal_call_stacks(entity, samples, connections, indent = ''):
    s = samples[entity]
    print('{}{} total={:.3f} self={:.3f} calls={} alloc={}'.format(indent, s.name, s.total_time, s.time, s.calls_count, s.alloc_bytes))
    children = connections.get(entity)
    if children:
        for n in children:
            reveal_call_stacks(entity=n, connections=connections, samples=samples, indent=indent + '    ')

def main():
    import argparse, sys
    arguments = argparse.ArgumentParser()
    arguments.add_argument('--file', '-f', required=True)
    options = arguments.parse_args(sys.argv[1:])
    with open(options.file, 'rb') as fp:
        fp.seek(0, os.SEEK_END)
        length = fp.tell()
        fp.seek(0, os.SEEK_SET)
        assert fp.read(3) == b'PFC'
        timestamp, = unpack('=Q', fp.read(8))
        print('create_time = {:.6f}'.format(timestamp / 1e6))

        offset, = unpack('=i', fp.read(4))
        fp.seek(offset)

        # read string
        strmap = []
        string_count, = unpack('=i', fp.read(4))
        for _ in range(string_count):
            value = fp.read(unpack('=i', fp.read(4))[0]).decode('utf-8')
            strmap.append(value)
        print(strmap)

        complete_timestamp, = unpack('=Q', fp.read(8))

        fp.seek(11 + 4, os.SEEK_SET)
        print('size={:,} pos={} elapse={:.3f}'.format(length, fp.tell(), (complete_timestamp - timestamp)/1e6))

        prev_frame_index = -1
        while fp.tell() < offset:
            frame_index, = unpack('=i', fp.read(4))
            assert prev_frame_index == -1 or frame_index - prev_frame_index == 1
            frame_time, = unpack('=f', fp.read(4))
            frame_fps, = unpack('=f', fp.read(4))
            print('frame_index={} frame_time={:.3f} frame_fps={:.1f}s'.format(frame_index, frame_time, frame_fps))
            samples = {}
            relations = {}
            connections = {}

            sample_count, = unpack('=i', fp.read(4))
            # print('sample_count={}'.format(sample_count))
            for _ in range(sample_count):
                sample_id, = unpack('=i', fp.read(4))
                name_index, = unpack('=i', fp.read(4))
                name = strmap[name_index]
                calls_count, = unpack('=i', fp.read(4))
                alloc_bytes, = unpack('=i', fp.read(4))
                total_time, = unpack('=f', fp.read(4))
                self_time, = unpack('=f', fp.read(4))
                samples[sample_id] = StackSample(id=sample_id, name=name, calls_count=calls_count, alloc_bytes=alloc_bytes, total_time=total_time, time=self_time)
                # print(samples[sample_id])
            relation_count, = unpack('=i', fp.read(4))
            for _ in range(relation_count):
                key, = unpack('=i', fp.read(4))
                value, = unpack('=i', fp.read(4))
                relations[key] = value
                if value not in connections:
                    connections[value] = []
                connections[value].append(key)
            # print(relations)
            # print(connections)

            # for entity, _ in connections.items():
            #     reveal_call_stacks(entity, samples, connections)
            magic, = unpack('=I', fp.read(4))
            assert magic == 0x12345678
            prev_frame_index = frame_index
            # break

if __name__ == '__main__':
    main()
