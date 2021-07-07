#!/bin/bash

#First argument is the log file
READ=0;READ_SIZE=0
WRITE=0;WRITE_SIZE=0
LOCK=0;UNLOCK=0;OPEN_LOCK=0;CLOSE=0;REMOVE=0;
MAX_SIZE=0;MAX_FILES=0;MAX_CONN=0;CACHE_MISS=0;
OP_THREAD[0]=0;

print_state(){
  local CURR=$(($1*100/$2))
  local str="["
  local count=30
  local i=0
  for ((;i < ($1*$count/$2); i++)); do
    str+="#"
  done
  for ((;i < count; i++)); do
    str+="."
  done
  str+="] "$CURR"% - line:"$1"\r"
  echo -e -n $str
}

clear_state(){
  printf "                                               \r"
}

increment_thread(){
  if [ ${#OP_THREAD[@]} -lt $(($1+1)) ]; then
    local CURR=${#OP_THREAD[@]}
    for((i=0; i < $(($1 - $CURR)); i++)); do
      OP_THREAD[$(($CURR + $i))]=0
    done
  fi
  OP_THREAD[$1]=$((${OP_THREAD[$1]}+1))
}

parse_operation(){
  if [ $(eval "echo \"$1\" | grep \"OpenLock\"") ]; then
    ((OPEN_LOCK++))
  elif [ $(eval "echo \"$1\" | grep \"Unlock\"") ]; then
    ((UNLOCK++))
  elif [ $(eval "echo \"$1\" | grep \"Lock\"") ]; then
    ((LOCK++))
  elif [ $(eval "echo \"$1\" | grep \"Close\"") ]; then
    ((CLOSE++))
  elif [ $(eval "echo \"$1\" | grep \"Remove\"") ]; then
    ((REMOVE++))
  fi
}

parse_line(){
  #line format 1: [timestamp] Thread:$$$ Read:$$$ Write:$$$ Client-Op:[$$$-$$$] Outcome:$$$ File:$$$ 
  #line format 2: [timestamp] MAIN MaxSize:$$$ MaxFiles:$$$ MaxConn:$$$ CacheMiss:$$$
  local FIRST=$(eval "echo \"$1\" | cut - -f 2")
  local TEMP=""
  if [ "$FIRST" = "MAIN" ]; then
    TEMP=$(eval "echo \"$1\" | cut - -f 3")
    MAX_SIZE=$(eval "echo \"$TEMP\" | cut - -d : -f 2")
    TEMP=$(eval "echo \"$1\" | cut - -f 4")
    MAX_FILES=$(eval "echo \"$TEMP\" | cut - -d : -f 2")
    TEMP=$(eval "echo \"$1\" | cut - -f 5")
    MAX_CONN=$(eval "echo \"$TEMP\" | cut - -d : -f 2")
    TEMP=$(eval "echo \"$1\" | cut - -f 6")
    CACHE_MISS=$(eval "echo \"$TEMP\" | cut - -d : -f 2")
  elif [ $(eval "echo \"$FIRST\" | grep \"Thread\"") ]; then
    #Thread:$$$
    TEMP=$(eval "echo \"$FIRST\" | cut - -d ':' -f 2")
    increment_thread "$TEMP"
    
    #Client-Operation:[$$$-$$$]
    TEMP=$(eval "echo \"$1\" | cut - -f 5")
    TEMP=$(eval "echo \"$TEMP\" | cut - -d ':' -f 2")
    parse_operation $TEMP
    
    #Read:$$$
    TEMP=$(eval "echo \"$1\" | cut - -f 3")
    TEMP=$(eval "echo \"$TEMP\" | cut - -d ':' -f 2")
    if [ $TEMP -gt 0 ]; then
      ((READ++))
      READ_SIZE=$(($READ_SIZE + $TEMP))
    fi
    
    #Write:$$$
    TEMP=$(eval "echo \"$1\" | cut - -f 4")
    TEMP=$(eval "echo \"$TEMP\" | cut - -d ':' -f 2")
    if [ $TEMP -gt 0 ]; then
      ((WRITE++))
      WRITE_SIZE=$(($WRITE_SIZE + $TEMP))
    fi
  fi
}

print_data(){
  if [ $READ -ne 0 ]; then
    READ_SIZE_MED=$(eval "echo \"scale=2; $READ_SIZE / $READ\" | bc -lq")
  else
    READ_SIZE_MED=0.00
  fi
  printf "N. Read: %s; Dimensione media: %s\n" $READ $READ_SIZE_MED

  if [ $WRITE -ne 0 ]; then
    WRITE_SIZE_MED=$(eval "echo \"scale=2; $WRITE_SIZE / $WRITE\" | bc -lq")
  else
    WRITE_SIZE_MED=0.00
  fi
  printf "N. Write: %s; Dimensione media: %s\n" $WRITE $WRITE_SIZE_MED

  printf "N. Lock: %s; N. OpenLock: %s\n" $LOCK $OPEN_LOCK

  printf "N. Unlock: %s; N. Remove: %s; N. Close: %s\n" $UNLOCK $REMOVE $CLOSE

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
  clear_state
  printf "lines parsed: %s\n" $COUNT
  print_data
  exit 0
}

trap print_exit SIGINT

LINES=$(eval "wc -l \"$1\"")
COUNT=0
print_state $COUNT $LINES 
while read -r line; do
  parse_line "$line"
  ((COUNT++))
  print_state $COUNT $LINES
done < $1

clear_state

print_data

exit 0
