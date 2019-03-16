#!/usr/bin/env python3
from memory.profiler.serialize import MemorySnapshotReader

def main():
    import argparse,sys
    arguments = argparse.ArgumentParser()
    arguments.add_argument('--file-path', '-f', required=True)
    arguments.add_argument('--debug', '-d', action='store_true')
    options = arguments.parse_args(sys.argv[1:])
    reader = MemorySnapshotReader(debug=options.debug)
    data = reader.read(file_path=options.file_path)
    assert reader.cached_ptr
    print(reader.cached_ptr.dump())
    print(data.dump())

if __name__ == '__main__':
    main()