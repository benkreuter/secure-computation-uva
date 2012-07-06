#!/bin/sh

m4 $1 | ./parser $1.map 2.5 1000000 $1.inputs $1.stats >$1.t1
./identitygates $1.map 1000000 $1.stats <$1.t1 >$1.t2
sort -n -r -k 2 $1.t2 | ./deadgates $1.t2 1000000 $1.map $((`wc -l $1.inputs | awk '{print $1}'` - 1)) $1.stats > $1.last 
 ./binary $1.last $1.bin $1.inputs $1.stats
