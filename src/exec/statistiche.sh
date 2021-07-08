#!/bin/bash

#First argument is the log file
READ=0;READ_SIZE=0;
WRITE=0;WRITE_SIZE=0;
APPEND=0;APPEND_SIZE=0;
LOCK=0;UNLOCK=0;CREATE_LOCK=0;OPEN_NOLOCK=0;
OPEN_LOCK=0;CLOSE=0;REMOVE=0;
MAX_SIZE=0;MAX_FILES=0;MAX_CONN=0;CACHE_MISS=0;
SUCCESS=0;
OP_THREAD[0]=0;

print_data(){
  if [ $READ -ne 0 ]; then
    READ_SIZE_MED=$(eval "echo \"scale=2; $READ_SIZE / $READ\" | bc -lq")
  else
    READ_SIZE_MED=0.00
  fi
  printf "N. Read: %s; Dimensione media: %s bytes\n" $READ $READ_SIZE_MED

  if [ $WRITE -ne 0 ]; then
    WRITE_SIZE_MED=$(eval "echo \"scale=2; $WRITE_SIZE / $WRITE\" | bc -lq")
  else
    WRITE_SIZE_MED=0.00
  fi
  printf "N. Write: %s; Dimensione media: %s bytes\n" $WRITE $WRITE_SIZE_MED
  
  if [ $APPEND -ne 0 ]; then
    APPEND_SIZE_MED=$(eval "echo \"scale=2; $APPEND_SIZE / $APPEND\" | bc -lq")
  else
    APPEND_SIZE_MED=0.00
  fi
  printf "N. Append: %s; Dimensione media: %s bytes\n" $APPEND $APPEND_SIZE_MED

  printf "N. Lock: %s; N. OpenLock: %s\n" $LOCK $OPEN_LOCK

  printf "N. Open (no lock): %s; N. CreateLock: %s\n" $OPEN_NOLOCK $CREATE_LOCK

  printf "N. Unlock: %s; N. Remove: %s; N. Close: %s\n" $UNLOCK $REMOVE $CLOSE

  printf "N. Success: %s\n" $SUCCESS

  MAX_SIZE_MB=$(eval "echo \"scale=4; $MAX_SIZE.0 / 1048576.0\" | bc -lq")
  printf "Max Size: %s bytes (%s Mb)\n" $MAX_SIZE $MAX_SIZE_MB

  printf "Max Files: %s\n" $MAX_FILES

  printf "Esecuzioni rimpiazzamento: %s\n" $CACHE_MISS

  printf "Max Connessioni: %s\n" $MAX_CONN

  for ((i=0; i < ${#OP_THREAD[@]}; i++ )); do
    printf "Operazioni Thread N. %d : %s\n" $(($i+1)) ${OP_THREAD[$i]}
  done
}

print_exit(){
  print_data
  exit 0
}

trap print_exit SIGINT

#line format 1: [timestamp] Thread:$$$ Read:$$$ Write:$$$ Client-Op:[$$$-$$$] Outcome:$$$ File:$$$
TH_MAX=50
for((i=0;i<$TH_MAX;i++)); do
  CNT=$(eval "grep -o \"Thread:$i\" $1 | wc -l")
  if [ $CNT -ne 0 ]; then
    OP_THREAD[$i]=$CNT
  fi
done

TEMP=$(eval "grep -o \"Read:[1-9][0-9]*\" $1 | cut - -d ':' -f 2")
READ_SIZE=$(eval "echo -n \"$TEMP\" | paste -sd+ - | bc -q")
READ=$(eval "echo -n \"$TEMP\" | wc -l")

TEMP=$(eval "grep -o 'Write:[1-9][0-9]*\sClient\-Op:\[[0-9][0-9]*\-Write' log.txt | cut - -d ':' -f 2 | cut - -f 1")
WRITE_SIZE=$(eval "echo -n \"$TEMP\" | paste -sd+ - | bc -q")
WRITE=$(eval "echo -n \"$TEMP\" | wc -l")

TEMP=$(eval "grep -o 'Write:[1-9][0-9]*\sClient\-Op:\[[0-9][0-9]*\-Append' log.txt | cut - -d ':' -f 2 | cut - -f 1")
APPEND_SIZE=$(eval "echo -n \"$TEMP\" | paste -sd+ - | bc -q")
APPEND=$(eval "echo -n \"$TEMP\" | wc -l")

LOCK=$(eval "grep -o \"\-Lock\" $1 | wc -l")
OPEN_LOCK=$(eval "grep -o \"\-OpenLock\" $1 | wc -l")
OPEN_NOLOCK=$(eval "grep -o \"\-Open \" $1 | wc -l")
CREATE_LOCK=$(eval "grep -o \"\-CreateLock\" $1 | wc -l")
UNLOCK=$(eval "grep -o \"\-Unlock\" $1 | wc -l")
CLOSE=$(eval "grep -o \"\-Close\" $1 | wc -l")
REMOVE=$(eval "grep -o \"\-Remove\" $1 | wc -l")
SUCCESS=$(eval "grep -o \"Success.\" $1 | wc -l")

#line format 2: [timestamp] MAIN MaxSize:$$$ MaxFiles:$$$ MaxConn:$$$ CacheMiss:$$$
MAX_SIZE=$(eval "grep -o \"MaxSize:[0-9]*\" $1 | cut - -d ':' -f 2")
MAX_FILES=$(eval "grep -o \"MaxFiles:[0-9]*\" $1 | cut - -d ':' -f 2")
MAX_CONN=$(eval "grep -o \"MaxConn:[0-9]*\" $1 | cut - -d ':' -f 2")
CACHE_MISS=$(eval "grep -o \"CacheMiss:[0-9]*\" $1 | cut - -d ':' -f 2")

print_data

exit 0
