#!/bin/bash
for buffSize in 74 37
do
    for protocol in "ndp" "homa" "homatr" 
    do 
        nice ./waf --run "scratch/nanopu-protocol-eval --protocol=$protocol --bufferSize=$buffSize"
    done
done
