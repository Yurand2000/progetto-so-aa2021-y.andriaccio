#!/bin/bash

#First argument is the log file
READ=0;READ_SIZE=0
WRITE=0;WRITE_SIZE=0
LOCK=0;UNLOCK=0;OPEN_LOCK=0;CLOSE=0;
MAX_SIZE=0;MAX_FILES=0;MAX_CONN=0;CACHE_MISS=0;
OP_THREAD[0] = 0;

increment_thread()
{
  if [ ${#OP_THREAD[@]} -lt $1 ]; then
    local CURR= ${#OP_THREAD[@]}
    for((local i=0; i < [$1 - CURR]; i++)); do
      {OP_THREAD[[$CURR + $i]]}=0
    done
  fi
  {OP_THREAD[$1]}++
}

parse_operation()
{
  
  if [(grep - "Lock"< $1)]; then
    LOCK++
  elif [(grep - "Unlock"< $1)]; then
    UNLOCK++
  elif [(grep - "OpenLock"< $1)]; then
    OPEN_LOCK++
  elif [(grep - "Close"< $1)]; then
    CLOSE++
  fi
}

parse_line()
{
  #line format 1: [timestamp] Thread:$$$ File:$$$ Operation:$$$ Outcome:$$$ ReadSize:$$$ WriteSize:$$$ 
  #line format 2: [timestamp] MAIN MaxSize:$$$ MaxFiles:$$$ MaxConn:$$$ CacheMiss:$$$
  local FIRST= cut - -d ' ' -f 1 < $1
  local TEMP=""
  if [ $FIRST == "MAIN" ]; then
    TEMP= cut - -d ' ' -f 2 < $1
    MAX_SIZE= cut - -d ':' -f 2 < $TEMP
    TEMP= cut - -d ' ' -f 3 < $1
    MAX_FILES= cut - -d ':' -f 2 < $TEMP
    TEMP= cut - -d ' ' -f 4 < $1
    MAX_CONN= cut - -d ':' -f 2 < $TEMP
    TEMP= cut - -d ' ' -f 5 < $1
    CACHE_MISS= cut - -d ':' -f 2 < $TEMP
  else
    #Thread:$$$
    TEMP= cut - -d ':' -f 2 < $FIRST
    increment_thread $TEMP
    
    #Operation:$$$
    TEMP= cut - -d ':' -f 4 < $1
    parse_operation $TEMP
    
    #ReadSize:$$$
    TEMP= cut - -d ' ' -f 6 < $1
    TEMP= cut - -d ':' -f 2 < $TEMP
    if [TEMP -gt 0]; then
      READ++
      READ_SIZE=[$READ_SIZE + $TEMP]
    fi
    
    #WriteSize:$$$
    TEMP= cut - -d ' ' -f 7 < $1
    TEMP= cut - -d ':' -f 2 < $TEMP
    if [TEMP -gt 0]; then
      WRITE++
      WRITE_SIZE=[$WRITE_SIZE + $TEMP]
    fi
  fi
}

while read line; do
  parse_line $line
done < $1

READ_SIZE_MED=bc -q "$READ / $READ_SIZE"
printf "N. Read: %s; Dimensione media: %s\n" $READ $

WRITE_SIZE_MED=bc -q "$WRITE / $WRITE_SIZE"
printf "N. Write: %s; Dimensione media: %s\n" $WRITE $WRITE_SIZE_MED

printf "N. Lock: %s; N. OpenLock: %s\n" $LOCK $OPEN_LOCK

printf "N. Unlock: %s; N. Close: %s\n" $UNLOCK $CLOSE

MAX_SIZE_MB=bc -q "$MAX_SIZE / 1048576"
printf "Max Size: %s\n" $MAX_SIZE_MB

printf "Max Files: %s\n" $MAX_FILES

printf "Esecuzioni rimpiazzamento: %s\n" $CACHE_MISS

printf "Max Connessioni: %s\n" $MAX_CONN

for ((i=0; i < ${#OP_THREAD[@]}; i++ )); do
  printf "Operazioni Thread N. %s : %s\n" $i ${OP_THREAD[$i]}
done

exit 0