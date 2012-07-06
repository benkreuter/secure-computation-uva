#!/bin/bash

mpirun -n 1 ./evl 80 256 ./circuits/fast-aes.circuit.bin.hash-free inp.txt 69.30.63.212 5000 1
mpirun -n 2 ./evl 80 256 ./circuits/fast-aes.circuit.bin.hash-free inp.txt 69.30.63.212 5000 1
mpirun -n 4 ./evl 80 256 ./circuits/fast-aes.circuit.bin.hash-free inp.txt 69.30.63.212 5000 1
mpirun -n 1 ./evl 80 256 ./circuits/fast-aes.circuit.bin.hash-free inp.txt 69.30.63.212 5000 2
mpirun -n 2 ./evl 80 256 ./circuits/fast-aes.circuit.bin.hash-free inp.txt 69.30.63.212 5000 2
mpirun -n 4 ./evl 80 256 ./circuits/fast-aes.circuit.bin.hash-free inp.txt 69.30.63.212 5000 2
mpirun -n 1 ./evl 80 1 ./circuits/rsa.256.bin.hash-free inp.txt 69.30.63.212 5000 0
