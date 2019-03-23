#!/usr/bin/env python3

def main():
    import argparse, sys
    arguments = argparse.ArgumentParser()
    arguments.add_argument('--address-csv', '-f', nargs='+', required=True)
    options = arguments.parse_args(sys.argv[1:])
    import struct
    import os.path as p
    for address_csv in options.address_csv:
        address_csv = p.abspath(address_csv)
        output_path = open(p.join(p.dirname(address_csv), 'address.map'), 'wb')
        with open(address_csv, 'r') as fp:
            line_number = 0
            for line in fp.readlines():
                line_number += 1
                if line_number == 1: continue
                sep_index = 0
                sep_count = 0
                pair = []
                while len(pair) < 2:
                    new_index = line.find('"', sep_index + 1)
                    if sep_count % 2 == 0:
                        pair.append(line[sep_index+1:new_index])
                    sep_index = new_index
                    sep_count += 1
                name, address = pair # type: str, str
                address = int(address) # type: int
                print('0x{:x} {!r}'.format(address, name))
                output_path.write(struct.pack('>Q', address))
                name_data = name.encode('ascii')
                output_path.write(struct.pack('>H', len(name_data)))
                output_path.write(name_data)
            output_path.close()

if __name__ == '__main__':
    main()