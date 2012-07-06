#!/bin/bash

mpirun -n 1 ./gen 80 256 ./circuits/fast-aes.circuit.bin.hash-free inp.txt 69.30.63.212 5000 1
sleep 5
mpirun -n 2 ./gen 80 256 ./circuits/fast-aes.circuit.bin.hash-free inp.txt 69.30.63.212 5000 1
sleep 5
mpirun -n 4 ./gen 80 256 ./circuits/fast-aes.circuit.bin.hash-free inp.txt 69.30.63.212 5000 1
sleep 5
mpirun -n 1 ./gen 80 256 ./circuits/fast-aes.circuit.bin.hash-free inp.txt 69.30.63.212 5000 2
sleep 5
mpirun -n 2 ./gen 80 256 ./circuits/fast-aes.circuit.bin.hash-free inp.txt 69.30.63.212 5000 2
sleep 5
mpirun -n 4 ./gen 80 256 ./circuits/fast-aes.circuit.bin.hash-free inp.txt 69.30.63.212 5000 2
sleep 5
mpirun -n 1 ./gen 80 1 ./circuits/rsa.256.bin.hash-free inp.txt 69.30.63.212 5000 0
