#!/bin/bash
# $1: wait to kill time
# $2: #(nodes)
echo "==== PROCESS BEGIN ==="

echo sleep $1
sleep $1

echo pkill -SIGTERM stp
pkill -SIGTERM stp

echo sleep 1
sleep 1

echo "./dump_output.sh $2 > out/res_$2_nodes.txt"
./dump_output.sh $2 > out/res_$2_nodes.txt

echo "==== PROCESS FINISHED ==="