#!/bin/bash
for load in 0.8 0.5
do
    for simIdx in $(seq 0 $(($1-1))) 
    do 
        nice ./waf --run "scratch/homa-nanopu-paper-reproduction --load=$load --simIdx=$simIdx --duration=$2 --disableRtx --bdpPkts=$3" &
        sleep 10
    done
    wait
done