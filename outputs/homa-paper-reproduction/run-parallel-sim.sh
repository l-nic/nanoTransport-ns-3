#!/bin/bash
for load in 0.8 0.5
do
    for simIdx in $(seq 0 $(($1-1))) 
    do 
        nice ./waf --run "scratch/homa-paper-reproduction --duration=0.3 --load=$load --simIdx=$simIdx" &
        sleep 10
    done
    wait
done