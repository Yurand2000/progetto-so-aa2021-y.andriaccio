#!/bin/bash

CLIENT=./build/exec/client.out
SOCKFILE=./socket.sk
TESTDIR=src/tests/test3
CLIENT_OPT=" -f $SOCKFILE -t 0 -p "

OPS[0]="-W $TESTDIR/files/file_0.txt -r $TESTDIR/files/file_1.txt -d $TESTDIR/ret -l $TESTDIR/files/file_2.txt,$TESTDIR/files/file_3.txt -c $TESTDIR/files/file_2.txt,$TESTDIR/files/file_3.txt"
OPS[1]="-W $TESTDIR/files/file_1.txt -r $TESTDIR/files/file_2.txt -d $TESTDIR/ret -l $TESTDIR/files/file_3.txt,$TESTDIR/files/file_4.txt -c $TESTDIR/files/file_3.txt,$TESTDIR/files/file_4.txt"
OPS[2]="-W $TESTDIR/files/file_2.txt -r $TESTDIR/files/file_3.txt -d $TESTDIR/ret -l $TESTDIR/files/file_4.txt,$TESTDIR/files/file_0.txt -c $TESTDIR/files/file_4.txt,$TESTDIR/files/file_0.txt"
OPS[3]="-W $TESTDIR/files/file_3.txt -r $TESTDIR/files/file_4.txt -d $TESTDIR/ret -l $TESTDIR/files/file_0.txt,$TESTDIR/files/file_1.txt -c $TESTDIR/files/file_0.txt,$TESTDIR/files/file_1.txt"
OPS[4]="-W $TESTDIR/files/file_4.txt -r $TESTDIR/files/file_0.txt -d $TESTDIR/ret -l $TESTDIR/files/file_1.txt,$TESTDIR/files/file_2.txt -c $TESTDIR/files/file_1.txt,$TESTDIR/files/file_2.txt"
OPS[5]="-w $TESTDIR/files -Rn=5 -d $TESTDIR/ret -l $TESTDIR/files/file3.txt -a $TESTDIR/files/file3.txt -u $TESTDIR/files/file3.txt"
OP_SIZE=${#OPS[@]}

RUN=$CLIENT
RUN+=$CLIENT_OPT
for((;;)) do
  TEMP=$RUN
  RAND=$(od -vAn -N4 -t u4 < /dev/urandom)
  RAND=$(($RAND % $OP_SIZE))
  TEMP+=${OPS[$RAND]}
  eval "$TEMP"
  RET=$?
  if [ $RET -ne 0 ] ; then
    echo "Client returned an error: " $TEMP " " $RET
    break
  fi
done
