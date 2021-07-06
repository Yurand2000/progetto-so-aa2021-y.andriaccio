#!/bin/bash

$1/server.out -c ./src/tests/test2/test2.cfg &
SERVER=$!
./src/tests/test2/client.sh
kill -s SIGHUP $SERVER
