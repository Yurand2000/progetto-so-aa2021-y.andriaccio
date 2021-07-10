#!/bin/bash

if [ $1 = "lru" ]; then
  echo "Test 2; LRU Cache Miss Algorithm"
  ./build/exec/server.out -c ./src/tests/test2/test2.lru.cfg &
elif [ $1 = "lfu" ]; then
  echo "Test 2; LFU Cache Miss Algorithm"
  ./build/exec/server.out -c ./src/tests/test2/test2.lfu.cfg &
else
  echo "Test 2; FIFO Cache Miss Algorithm"
  ./build/exec/server.out -c ./src/tests/test2/test2.cfg &
fi

SERVER=$!
./src/tests/test2/client.sh
kill -s SIGHUP $SERVER
sleep 2
echo "Esecuzioni cachemiss di seguito:"
grep "CacheMiss" log.txt