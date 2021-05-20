#!/bin/bash

echo "clear tmp files..."
rm -rf trie*.log

number=1
echo "===== START ====="
while [ "$number" -le 5 ]; do
  echo -e "=== Number = $number"
  echo "exec: normal trie"
  ./build/prj_10 >> trie_normal.log
  sleep 1
  echo "exec: reduced trie"
  ./build/prj_10 -t >> trie_reduced.log
  sleep 1
  number=$((number + 1))
done
echo "===== FINISH ====="
