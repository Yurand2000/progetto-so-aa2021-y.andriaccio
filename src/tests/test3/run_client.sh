#!/bin/bash

CLIENT=./build/exec/client.out
SOCKFILE=./socket.sk
TESTDIR=src/tests/test3
CLIENT_OPT=" -f $SOCKFILE -t 0"

function rand_file(){
  echo -n "$TESTDIR/files/file_$(($RANDOM%200)).txt"
}

function rand_op(){
  OPT=$(($RANDOM%20))
  if [ $OPT -eq 0 ]; then
    echo -n "-Rn=$(($RANDOM%10)) -d $TESTDIR/ret"
  else
    echo -n "${OPS[$(($RANDOM%$OP_SIZE))]} $(rand_file)"
  fi
}

OPS[0]="-W"
OPS[1]="-l"
OPS[2]="-r"
OPS[3]="-c"
OPS[4]="-a"
OPS[5]="-u"
OP_SIZE=${#OPS[@]}

RUN=$CLIENT$CLIENT_OPT
function on_sigint(){
	sleep 2
  echo "Client $1 calls count: "$COUNT
  exit 0
}
trap on_sigint SIGINT
trap on_sigint SIGUSR1

COUNT=0
while true; do
  RAND=$(od -vAn -N4 -t u4 < /dev/urandom)
  RAND=$(($RAND % $OP_SIZE))

  CALL="$RUN $(rand_op) $(rand_op) $(rand_op) $(rand_op) $(rand_op) > /dev/null 2> /dev/null"
  eval "$CALL"
  RET=$?
  ((COUNT++))
  if [ $RET -ne 0 ] ; then
    echo "Client $1 returned an error: "$RET" - calls count: "$COUNT
    break
  fi
done
