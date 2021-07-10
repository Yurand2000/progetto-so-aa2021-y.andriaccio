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
for((i = 0; i < 4; i++))
	$CLIENT $CLIENT_OPT -W $TESTDIR/fill/file_$i.txt -a $TESTDIR/fill/file_$i.txt -u $TESTDIR/fill/file_$i.txt -D $TESTDIR/cmiss
for((i = 4; i < 8; i++))
	$CLIENT $CLIENT_OPT -W $TESTDIR/fill/file_$i.txt -D $TESTDIR/cmiss
echo "-------------------------"
echo "Cache miss"
$CLIENT $CLIENT_OPT -W $TESTDIR/data/file_0.txt -D $TESTDIR/cmiss
$CLIENT $CLIENT_OPT -W $TESTDIR/data/file_1.txt -D $TESTDIR/cmiss
$CLIENT $CLIENT_OPT -c $TESTDIR/fill/file_2.txt -a $TESTDIR/fill/file_3.txt,$TESTDIR/fill/file_4.txt
$CLIENT $CLIENT_OPT -W $TESTDIR/data/file_3.txt,$TESTDIR/data/file_4.txt,$TESTDIR/data/file_5.txt -D $TESTDIR/cmiss
sleep 1
echo "-------------------------"

exit 0
