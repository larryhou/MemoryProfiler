#!/usr/bin/env python3
import math, io

class Rect(object):
    def __init__(self):
        self.minX = 0
        self.maxX = 0
        self.minY = 0
        self.maxY = 0

def iterate_petal(length, angle, limit = 4, depth = 0): # type: (int, float, int, int)->list[tuple[float, float]]
    nodes = []
    if depth >= limit:
        return [(length * math.cos(angle), length * math.sin(angle))]
    delta = length / 3.0
    nodes += iterate_petal(length=delta, angle=angle, depth=depth + 1, limit=limit)
    nodes += iterate_petal(length=delta, angle=angle + math.pi / 3, depth=depth + 1, limit=limit)
    nodes += iterate_petal(length=delta, angle=angle - math.pi / 3, depth=depth + 1, limit=limit)
    nodes += iterate_petal(length=delta, angle=angle, depth=depth + 1, limit=limit)
    return nodes

def main():
    import argparse, sys
    arguments = argparse.ArgumentParser()
    arguments.add_argument('--dimension', '-d', default=1000, type=int)
    arguments.add_argument('--rotation', '-r', default=0, type=float)
    arguments.add_argument('--margin', '-m', default=4, type=float)
    arguments.add_argument('--level', '-l', default=4, type=int)
    options = arguments.parse_args(sys.argv[1:])
    dimension = options.dimension
    rotation = options.rotation / 180 * math.pi
    margin = options.margin

    posX, posY = 0, 0
    rect = Rect()
    rect.minX, rect.minY, rect.maxX, rect.maxY = 0, 0, 0, 0
    path = io.StringIO()
    for _ in range(3):
        points = iterate_petal(length=dimension, angle=rotation, limit=options.level)
        for p in points: # type: tuple[float, float]
            path.write(' l{:.2f} {:.2f}'.format(*p))
            posX += p[0]
            posY += p[1]
            if posX > rect.maxX: rect.maxX = posX
            if posX < rect.minX: rect.minX = posX
            if posY > rect.maxY: rect.maxY = posY
            if posY < rect.minY: rect.minY = posY
        rotation -= math.pi / 3 * 2
    path.write(' z')
    path.seek(0)

    buf = io.StringIO()
    buf.write('<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" version="1.1" viewBox="{:.2f} {:.2f} {:.2f} {:.2f}">\n'.format(
        rect.minX-margin, rect.minY-margin, rect.maxX - rect.minX + 2 * margin, rect.maxY - rect.minY + 2 * margin
    ))
    buf.write('<path style="stroke:rgb(255,0,0);stroke-width:1;fill:none" d="')
    buf.write('M{:.2f} {:.2f}'.format(0, 0))
    buf.write(path.read())
    buf.write('" />\n</svg>\n')
    buf.seek(0)
    print(buf.read())


if __name__ == '__main__':
    main()