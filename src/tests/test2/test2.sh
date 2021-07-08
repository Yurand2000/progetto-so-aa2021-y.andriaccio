#!/bin/bash

if [ $2 = "lru" ]; then
  echo "Test 2; LRU Cache Miss Algorithm"
  $1/server.out -c ./src/tests/test2/test2.lru.cfg &
elif [ $2 = "lfu" ]; then
  echo "Test 2; LFU Cache Miss Algorithm"
  $1/server.out -c ./src/tests/test2/test2.lfu.cfg &
else
  echo "Test 2; FIFO Cache Miss Algorithm"
  $1/server.out -c ./src/tests/test2/test2.cfg &
fi

SERVER=$!
./src/tests/test2/client.sh
kill -s SIGHUP $SERVER
