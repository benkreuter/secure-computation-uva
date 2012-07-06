#!/usr/bin/python

import sys
import anydbm

inf = open(sys.argv[1], "r")
lastUsed = dict()
inMem = {}

curline = 0

for line in inf:
    tokens = line.split(" ")
    lastUsed[int(tokens[0])] = curline
    if not (tokens[1] == "input" or tokens[1] == "output"):
        s = n = cnt = 0
        for tok in tokens:
            if tok == "inputs":
                s = cnt+2
            if tok == "]" and tokens[1] == "output":
                comment = tokens[cnt+1]
            cnt = cnt+1
            
        for i in range(0,int(tokens[3])):
            lastUsed[int(tokens[s+i])] = curline

    curline = curline + 1

inMem = [0]*curline
closed = list(lastUsed.values())
closed.sort()
c=0
begin = iter(closed)
q = begin.next()
max = 0

for i in range(0,curline-1):
    c = c+1
    while q <= i:
        c = c - 1
        q = begin.next()
    if c > max:
        max = c

print max

