#!/usr/bin/env python3
from memory.profiler.serialize import MemorySnapshotReader
from memory.profiler.crawler import MemorySnapshotCrawler
import os, struct

def compare_address_map(crawler:MemorySnapshotCrawler):
    address_map = {}
    address_list = []
    with open('address.map', 'rb') as fp:
        fp.seek(0, os.SEEK_END)
        length = fp.tell()
        fp.seek(0)
        while fp.tell() < length:
            address = struct.unpack('>Q', fp.read(8))[0] # type: int
            size = struct.unpack('>H', fp.read(2))[0] # type: int
            name = fp.read(size).decode('ascii')
            address_map[address] = name
            address_list.append(address)
    object_map = {}
    for mo in crawler.managed_objects:
        object_map[mo.address] = mo
    for addr in address_list:
        if addr not in object_map:
            print('* 0x{:x} {!r}'.format(addr, address_map.get(addr)))

def main():
    import argparse,sys
    arguments = argparse.ArgumentParser()
    arguments.add_argument('--file-path', '-f', required=True)
    arguments.add_argument('--debug', '-d', action='store_true')
    options = arguments.parse_args(sys.argv[1:])
    reader = MemorySnapshotReader(debug=options.debug)

    data = reader.read(file_path=options.file_path)
    # data.generate_type_module()
    assert reader.cached_ptr
    print(reader.cached_ptr.dump())
    crawler = MemorySnapshotCrawler(snapshot=data)
    print(data.dump())
    crawler.crawl()

    compare_address_map(crawler=crawler)

if __name__ == '__main__':
    main()