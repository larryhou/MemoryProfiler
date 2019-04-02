#!/usr/bin/env python3

def permuate(array, visit = []): # type: (list[int], list[int])->list[int]
    result = []
    if not array:
        result.append(visit)
    for n in range(len(array)):
        item  = array[n]
        result += permuate(array=array[:n] + array[n+1:], visit=visit+[item])
    return result

if __name__ == '__main__':
    import argparse, sys
    arguments = argparse.ArgumentParser()
    arguments.add_argument('--field-size', '-s', nargs='+', type=int, required=True)
    options = arguments.parse_args(sys.argv[1:])

    max_field_size = 0
    for field_size in options.field_size:
        if field_size > max_field_size: max_field_size = field_size

    def get_candicate_key(candidate):
        return '_'.join([str(x) for x in candidate])

    def align_address(address:int, algin:int)->int:
        return (address + algin - 1) & (~(algin - 1))

    result = []
    unique_map = {}
    for candidate in permuate(array=options.field_size):
        key = get_candicate_key(candidate)
        if key in unique_map: continue
        unique_map[key] = candidate
        address = 0
        for size in candidate:
            if address % size != 0:
                address = align_address(address, size) # align field
            address += size
        address = align_address(address, max_field_size) # align object
        result.append((address, candidate))
    import operator
    result.sort(key=operator.itemgetter(0))
    for size, candidate in result:
        print(size, candidate)




