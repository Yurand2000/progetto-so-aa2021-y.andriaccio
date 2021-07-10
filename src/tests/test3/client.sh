#!/bin/bash

TESTDIR=src/tests/test3

#create the necessary folders, suppress their output.
rm -r $TESTDIR/ret > /dev/null 2> /dev/null
mkdir $TESTDIR/ret > /dev/null 2> /dev/null

sleep 2

MAX_CLIENTS=11
for((i = 0; i < $MAX_CLIENTS; i++)); do
	$TESTDIR/run_client.sh $i &
	CLIENTS[$i]=$!
done

sleep 30

for((i = 0; i < $MAX_CLIENTS; i++)); do
	kill -s SIGUSR1 ${CLIENTS[$i]} > /dev/null 2> /dev/null
done

exit 0
