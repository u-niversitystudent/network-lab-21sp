#!/bin/bash

echo "remove temp files..."
rm -rf out/
mkdir out/
rm test*
echo "add test file 'test.txt' via echo"
echo "Neque porro quisquam est qui dolorem ipsum quia dolor sit amet, consectetur, adipisci velit..." >> test.txt
echo "process finished"

