#!/bin/bash

mpirun -n 4 ./evl 80 4 ./circuits/fast-aes.circuit.bin.hash-free inp.txt 69.30.63.212 5000 1
