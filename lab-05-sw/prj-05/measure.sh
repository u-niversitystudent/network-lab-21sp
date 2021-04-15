#!/bin/bash

number=1
while [ "$number" -le 5 ]; do
  echo -e "------\nNumber = $number"
  iperf -c $1 -t 30
  number=$((number + 1))
  sleep 1
done
