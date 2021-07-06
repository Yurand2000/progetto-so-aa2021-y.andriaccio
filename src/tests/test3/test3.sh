#!/bin/bash

$1/server.out -c ./src/tests/test3/test3.cfg &
SERVER=$!
./src/tests/test3/client.sh &
sleep 7
kill -s SIGINT $SERVER
