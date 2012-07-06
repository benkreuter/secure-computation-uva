#!/usr/bin/python

import sys
import struct

inf = open(sys.argv[1], "r")
outf = open(sys.argv[1] + ".cnt", "wb")
count = dict()
gates = {}

curline = 0

for line in inf:
    tokens = line.split(" ")
    count[int(tokens[0])] = 0
    if not (tokens[1] == "input" or tokens[1] == "output"):
        s = n = cnt = 0
        for tok in tokens:
            if tok == "inputs":
                s = cnt+2
            if tok == "]" and tokens[1] == "output":
                comment = tokens[cnt+1]
            cnt = cnt+1
            
        for i in range(0,int(tokens[3])):
            count[int(tokens[s+i])] = count[int(tokens[s+i])]+1
# We should be able to perform forwards and backwards optimizations by
# using a pair of stacks, and popping off of one while pushing onto
# the other.

    curline = curline + 1

gates = [(0,0,0,0)]*curline

for i in range(0,curline):
    outf.write(struct.pack("<Q", count[i]))

