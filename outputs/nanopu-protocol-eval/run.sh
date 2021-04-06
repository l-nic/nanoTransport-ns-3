#!/bin/bash
# for buffSize in 74 37
# for buffSize in 91 53
for buffSize in 110 58
do
    for protocol in "ndp" "homa" "homatr" 
    do 
        nice ./waf --run "scratch/nanopu-protocol-eval --protocol=$protocol --bufferSize=$buffSize --delay=131ns --initialCredit=12 --rto=6.0 --minMsgSize=18 --mtu=1088"
    done
done
