#!/bin/bash

number=0
while [ "$number" -lt 5 ]; do
  echo "Number = $number"
  wget http://10.0.0.2/out/100MB.dat -P out/
  number=$((number + 1))
  sleep 2
done
