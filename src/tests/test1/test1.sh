#!/bin/bash

valgrind --leak-check=full $1/server.out -c ./src/tests/test1/test1.cfg &
SERVER=$!
./src/tests/test1/client.sh
kill -s SIGHUP $SERVER
