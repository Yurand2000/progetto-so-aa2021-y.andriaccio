#!/bin/bash

CLIENT=./build/exec/client.out
SOCKFILE=./socket.sk
TESTDIR=src/tests/test1
CLIENT_OPT="-f $SOCKFILE -t 200 -p"

#create the necessary folders, suppress their output.
rm -r $TESTDIR/return > /dev/null 2> /dev/null
mkdir $TESTDIR/return > /dev/null 2> /dev/null
rm -r $TESTDIR/return_all > /dev/null 2> /dev/null
mkdir $TESTDIR/return_all > /dev/null 2> /dev/null
rm -r $TESTDIR/cmiss > /dev/null 2> /dev/null
mkdir $TESTDIR/cmiss > /dev/null 2> /dev/null

sleep 5

#launch clients
echo "--------------------------------"
echo "OPERATION: -w $TESTDIR/writeall -D $TESTDIR/cmiss"
$CLIENT $CLIENT_OPT -w $TESTDIR/writeall -D $TESTDIR/cmiss
echo "--------------------------------"
echo "OPERATION: -w $TESTDIR/writeall_part,n=1 -D $TESTDIR/cmiss"
$CLIENT $CLIENT_OPT -w $TESTDIR/writeall_part,n=1 -D $TESTDIR/cmiss
echo "--------------------------------"
echo "OPERATION: -W $TESTDIR/file1.txt,$TESTDIR/file2.txt -D $TESTDIR/cmiss"
$CLIENT $CLIENT_OPT -W $TESTDIR/file1.txt,$TESTDIR/file2.txt -D $TESTDIR/cmiss
echo "--------------------------------"
echo "OPERATION: -l $TESTDIR/file1.txt -r $TESTDIR/file1.txt -d $TESTDIR/return -u $TESTDIR/file1.txt"
$CLIENT $CLIENT_OPT -l $TESTDIR/file1.txt -r $TESTDIR/file1.txt -d $TESTDIR/return -u $TESTDIR/file1.txt
echo "--------------------------------"
echo "OPERATION: -a $TESTDIR/file1.txt -D $TESTDIR/cmiss"
$CLIENT $CLIENT_OPT -a $TESTDIR/file1.txt -D $TESTDIR/cmiss
echo "--------------------------------"
echo "OPERATION: -R -d $TESTDIR/return_all"
$CLIENT $CLIENT_OPT -R -d $TESTDIR/return_all
echo "--------------------------------"
echo "OPERATION: -c $TESTDIR/file1.txt,$TESTDIR/file2.txt"
$CLIENT $CLIENT_OPT -c $TESTDIR/file1.txt,$TESTDIR/file2.txt
echo "--------------------------------"

exit 0
