#!/bin/bash
# for buffSize in 74 37
for buffSize in 91 53
do
    for protocol in "ndp" "homa" "homatr" 
    do 
        nice ./waf --run "scratch/nanopu-protocol-eval --protocol=$protocol --bufferSize=$buffSize --delay=88ns --initialCredit=10 --rto=6.0 --minMsgSize=15 --mtu=1088"
    done
done
