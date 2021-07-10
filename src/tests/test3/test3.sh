#!/bin/bash

./build/exec/server.out -c ./src/tests/test3/test3.cfg &
SERVER=$!
./src/tests/test3/client.sh &
sleep 32
kill -s SIGINT $SERVER
