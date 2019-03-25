#!/usr/bin/env python3
from memory.profiler.serialize import MemorySnapshotReader, NativeMemoryRef
from memory.profiler.crawler import MemorySnapshotCrawler
import os, struct

def dump_missing_manged_objects(crawler:MemorySnapshotCrawler):
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
    missing_list = []
    for addr in address_list:
        if addr not in object_map:
            missing_list.append(addr)
    missing_count = len(missing_list)
    if missing_count == 0: return
    import math
    digit_count = math.ceil(math.log(missing_count, 10))
    print_format = '[{:%d}/{}]'%digit_count

    for n in range(len(missing_list)):
        addr = missing_list[n]
        print('* {} 0x{:x} {!r}'.format(print_format.format(n+1, missing_count), addr, address_map.get(addr)))

def dump_texture_2d(reader:MemorySnapshotReader):
    native_objects = reader.snapshot.nativeObjects
    texture_2d_type_index = reader.snapshot.nativeTypeIndex.Texture2D
    export_path = '__texture2d'
    if not os.path.exists(export_path): os.makedirs(export_path)
    for no in native_objects:
        if no.nativeTypeArrayIndex == texture_2d_type_index:
            file_path = '{}/{:08x}.tex'.format(export_path, no.nativeObjectAddress)
            ref = reader.native_memory_map.get(no.nativeObjectAddress) # type: NativeMemoryRef
            if ref:
                with open(file_path, 'wb') as fp: fp.write(ref.read())

def main():
    import argparse,sys
    arguments = argparse.ArgumentParser()
    arguments.add_argument('--file-path', '-f', required=True)
    arguments.add_argument('--debug', '-d', action='store_true')
    arguments.add_argument('--missing', '-m', action='store_true')
    arguments.add_argument('--texture-2d', '-t', action='store_true')

    options = arguments.parse_args(sys.argv[1:])
    reader = MemorySnapshotReader(debug=options.debug)

    data = reader.read(file_path=options.file_path)
    # data.generate_type_module()
    crawler = MemorySnapshotCrawler(snapshot=data)
    print(data.dump())
    crawler.crawl()

    if options.missing:
        dump_missing_manged_objects(crawler=crawler)
    if options.texture_2d:
        dump_texture_2d(reader=reader)

if __name__ == '__main__':
    main()