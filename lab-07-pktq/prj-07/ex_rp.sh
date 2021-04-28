#!/bin/bash

if [ $# -lt 1 ] ; then
    echo "USAGE: $0 test from multiple params"
    echo "E.g.: $0 50 100 150"
    exit 1
fi

time=$(date "+%Y-%m-%d %H:%M:%S")
echo $time

echo "=====PROCESS BEGIN====="


for i in "$@";
do
    if [[ "$i" =~ [0-9\.]* ]] ;
    then
        sleep 5
        echo "===SET MAXQ AS $i==="
        
        echo "rm -rf qlen-$i"
        rm -rf qlen-$i

        echo "sudo python ./reproduce_bufferbloat.py -q $i"
        sudo python ./reproduce_bufferbloat.py -q $i

        sleep 5
        echo "mv iperf_result.txt qlen-$i/iperf_result.txt"
        mv iperf_result.txt qlen-$i/iperf_result.txt
    else
        exit 0
    fi
done

echo "=====PROCESS FINISHED====="
