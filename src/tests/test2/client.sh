#!/bin/bash

CLIENT=./build/exec/client.out
SOCKFILE=./socket.sk
TESTDIR=src/tests/test2
CLIENT_OPT="-f $SOCKFILE -t 50 -p"

#create the necessary folders, suppress their output.
rm -r $TESTDIR/cmiss > /dev/null 2> /dev/null
mkdir $TESTDIR/cmiss > /dev/null 2> /dev/null

sleep 3

#launch clients
echo "-------------------------"
echo "Fill the server with max files and capacity"
$CLIENT $CLIENT_OPT -w $TESTDIR/fill -D $TESTDIR/cmiss
echo "-------------------------"
echo "Cache miss 1"
$CLIENT $CLIENT_OPT -W $TESTDIR/data/file_0.txt -D $TESTDIR/cmiss
echo "-------------------------"
echo "Cache miss 2"
$CLIENT $CLIENT_OPT -c $TESTDIR/fill/file_0.txt -W $TESTDIR/data/file_1.txt -D $TESTDIR/cmiss &
$CLIENT $CLIENT_OPT -W $TESTDIR/data/file_4.txt -D $TESTDIR/cmiss &
$CLIENT $CLIENT_OPT -c $TESTDIR/fill/file_3.txt -W $TESTDIR/data/file_3.txt -D $TESTDIR/cmiss &
$CLIENT $CLIENT_OPT -W $TESTDIR/data/file_9.txt -D $TESTDIR/cmiss &
sleep 1
echo "-------------------------"

exit 0
