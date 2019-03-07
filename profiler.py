#!/usr/bin/env python3
from memory.profiler.serialize import MemorySnapshot

def main():
    import argparse,sys
    arguments = argparse.ArgumentParser()
    arguments.add_argument('--file-path', '-f', required=True)
    options = arguments.parse_args(sys.argv[1:])
    snapshot = MemorySnapshot()
    snapshot.load(file_path=options.file_path)

if __name__ == '__main__':
    main()