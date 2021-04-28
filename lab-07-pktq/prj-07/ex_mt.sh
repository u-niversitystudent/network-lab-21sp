#!/bin/bash

if [ $# -lt 1 ] ; then
    echo "USAGE: $0 test from multiple params"
    echo "E.g.: $0 taildrop red codel"
    exit 1
fi

time=$(date "+%Y-%m-%d %H:%M:%S")
echo $time

echo "=====PROCESS BEGIN====="

for i in "$@";
do
    if [[ "$i" =~ [a-z]* ]] ;
    then
        sleep 5
        echo "=====SET ALGORITHM AS $i====="

        echo "rm -rf algo-$i"
        rm -rf algo-$i

        echo "sudo python ./mitigate_bufferbloat.py -a $i"
        sudo python ./mitigate_bufferbloat.py -a $i

        sleep 5
        echo "mv iperf_result.txt algo-$i/iperf_result.txt"
        mv iperf_result.txt algo-$i/iperf_result.txt
    else
        exit 0
    fi
done

echo "=====PROCESS END====="
