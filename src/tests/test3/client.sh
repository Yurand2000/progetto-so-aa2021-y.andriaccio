#!/bin/bash

TESTDIR=src/tests/test3

#create the necessary folders, suppress their output.
rm -r $TESTDIR/ret > /dev/null 2> /dev/null
mkdir $TESTDIR/ret > /dev/null 2> /dev/null

sleep 5

MAX_CLIENTS=11
for((i = 0; i < $MAX_CLIENTS; i++)); do
	$TESTDIR/run_client.sh &
	CLIENTS[$i]=$!
done

sleep 5

for((i = 0; i < $MAX_CLIENTS; i++)); do
	kill ${CLIENTS[$i]}
done

exit 0
