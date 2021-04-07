#!/bin/bash

number=1
while [ "$number" -lt 6 ]; do
  echo -e "------\nNumber = $number"
  iperf -c $1 -t 30
  number=$((number + 1))
  sleep 2
done
