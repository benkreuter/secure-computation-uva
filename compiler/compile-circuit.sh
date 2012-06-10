#!/bin/sh

export USE_DB=1
export STORE_TYPE=$2
export USE_MMAP_CIR=$3
export WINDOW_SIZE=$4

if [ -z $STORE_TYPE ]
then
    export STORE_TYPE=3
fi

if [ -z $USE_MMAP_CIR ]
then
    export USE_MMAP_CIR=1
fi

if [ -z $WINDOW_SIZE ]
then
    export WINDOW_SIZE=10000000
fi

time m4 $1 | ./parser $1.map 2.5 $WINDOW_SIZE $1.inputs $1.stats $1.t1 $STORE_TYPE $USE_MMAP_CIR
#echo "1"
time ./identitygates $1.map $1.stats $1.map.1 $1.t2 $1.t1 $STORE_TYPE $USE_MMAP_CIR
#rm -f $1.map $1.t1
#echo "2"
#sort -n -r -k 2 $1.t2 | ./deadgates $1.t2 10000000 $1.map.1 $((`wc -l $1.inputs | awk '{print $1}'` - 1)) $1.stats $1.map.2 $1.map.3 > $1.last  #<$1.tmp
time ./deadgates $1.t2 10000000 $1.map.1 $((`wc -l $1.inputs | awk '{print $1}'` - 1)) $1.stats $1.map.2 $1.map.3  $1.last $STORE_TYPE $USE_MMAP_CIR
#rm -f $1.map.1 $1.map.2 $1.map.3 $1.t2 $1.tmp
#echo "3"
time ./binary $1.last $1.bin $1.inputs $1.stats $1.map.4 $STORE_TYPE $USE_MMAP_CIR
#rm -f $1.map.4 $1.stats $1.last
