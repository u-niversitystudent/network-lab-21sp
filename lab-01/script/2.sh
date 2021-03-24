#!/bin/bash

rm out/*.dat*
dd if=/dev/zero of=out/100MB.dat bs=100M count=1
exit
