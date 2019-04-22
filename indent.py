#!/usr/bin/env python3
import os
import os.path as p

def iter_dir(base_path:str, indent:str):
    node_list = os.listdir(base_path)
    for n in range(len(node_list)):
        name = node_list[n]
        nest_path = p.join(base_path, name)
        if n + 1 < len(node_list):
            print('{}├─{}'.format(indent, name))
        else:
            print('{}└─{}'.format(indent, name))
        if p.isdir(nest_path):
            if n + 1 < len(node_list):
                iter_dir(nest_path, indent=indent + '│  ')
            else:
                iter_dir(nest_path, indent=indent + '   ')

if __name__ == '__main__':
    iter_dir('memory', '')