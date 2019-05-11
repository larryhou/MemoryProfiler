#!/usr/bin/env python


if __name__ == "__main__":
    import os.path as p
    filepath = p.join(p.dirname(p.abspath(__file__)), 'meta.json')
    import json
    fp = open('meta.dot', 'w+')
    fp.write('digraph\n{\n')
    fp.write('   bgcolor=transparent;\n')
    data = json.load(fp=open(filepath))

    entities = []
    quota_count = 0
    for area in range(len(data)):
        item = data[area]
        entities.append('a{}'.format(area))
        name = item.get('name')
        properties = item.get('list')
        fp.write('    a{}[label="{}"];\n'.format(area, name))
        children = []
        for n in range(len(properties)):
            id = 'p{}_{}'.format(area, n)
            fp.write('    {}[label="{}"];\n'.format(id, properties[n]))
            children.append(id)
            quota_count += 1
        fp.write('    a{}->{{{}}};\n'.format(area, ' '.join(children)))
    fp.write('   Quotas->{{{}}};\n'.format(' '.join(entities)))
    fp.write('}\n')
    fp.seek(0)
    print(fp.read())
    fp.close()
    print(quota_count)