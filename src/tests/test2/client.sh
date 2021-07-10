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
for((i = 0; i < 4; i++)); do
	$CLIENT $CLIENT_OPT -W $TESTDIR/fill/file_$i.txt -D $TESTDIR/cmiss -a $TESTDIR/fill/file_$i.txt -D $TESTDIR/cmiss -u $TESTDIR/fill/file_$i.txt 
done
for((i = 4; i < 8; i++)); do
	$CLIENT $CLIENT_OPT -W $TESTDIR/fill/file_$i.txt -D $TESTDIR/cmiss
done
echo "-------------------------"
echo "Cache miss"
$CLIENT $CLIENT_OPT -W $TESTDIR/data/file_0.txt -D $TESTDIR/cmiss
$CLIENT $CLIENT_OPT -c $TESTDIR/fill/file_1.txt -a $TESTDIR/fill/file_3.txt,$TESTDIR/fill/file_4.txt -D $TESTDIR/cmiss
$CLIENT $CLIENT_OPT -W $TESTDIR/data/file_2.txt,$TESTDIR/data/file_3.txt,$TESTDIR/data/file_4.txt -D $TESTDIR/cmiss
sleep 1
echo "-------------------------"

exit 0
