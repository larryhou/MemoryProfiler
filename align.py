#!/usr/bin/env python3

def permuate(array, visit = []): # type: (list[int], list[int])->list[int]
    result = []
    if not array:
        result.append(visit)
    for n in range(len(array)):
        item  = array[n]
        result += permuate(array=array[:n] + array[n+1:], visit=visit+[item])
    return result

class PermutationIterator(object):
    def __init__(self, items): # type: (list)->None
        self.__items = items
        self.__size = len(items)
        assert self.__size >= 3
        self.__complete = False
        self.__alias = list(range(self.__size))
        self.__first = True

    def __transform(self, columns):
        return [self.__items[x] for x in columns]

    def __iter__(self):
        self.__alias = list(range(self.__size))
        self.__first = True
        self.__complete = False
        return self

    def __next__(self):
        items = self.__alias
        if self.__complete: raise StopIteration()
        if self.__first:
            self.__first = False
            return self.__transform(columns=items)
        if items[-1] > items[-2]:
            temp = items[-1]
            items[-1] = items[-2]
            items[-2] = temp
            return self.__transform(columns=items)
        index = self.__size - 3
        while index >= 0:
            value = items[index]
            partial = sorted(items[index:])
            for n in range(1, len(partial)):
                if partial[n] > value:
                    if n != 0:
                        temp = partial[n]
                        x = n - 1
                        while x >= 0:
                            partial[x + 1] = partial[x]
                            x -= 1
                        partial[0] = temp
                    items[index:] = partial
                    return self.__transform(columns=items)
            index -= 1
        if index < 0: self.__complete = True
        raise StopIteration()

if __name__ == '__main__':
    import argparse, sys
    arguments = argparse.ArgumentParser()
    arguments.add_argument('--field-size', '-s', nargs='+', type=int, required=True)
    arguments.add_argument('--debug', '-d', action='store_true')
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
    for candidate in iter(PermutationIterator(items=options.field_size)):
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
    if not options.debug: result = result[:5]
    for size, candidate in result:
        print(size, candidate)

    # iter_count = 0
    # for rank in iter(PermutationIterator(options.field_size)):
    #     iter_count += 1
    #     print('+', iter_count, rank)
    # level = len(options.field_size)
    # total_count = 1
    # while level > 1:
    #     total_count *= level
    #     level -= 1
    # assert iter_count == total_count, 'expect={} but={}'.format(total_count, iter_count)





