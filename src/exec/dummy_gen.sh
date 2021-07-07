#!/bin/bash

print_state(){
  local str="\r["
  local count=30
  local i=0
  for ((;i < ($1*$count/$2); i++)); do
    str+="#"
  done
  for ((;i < count; i++)); do
    str+="."
  done
  str+="] "
  let a=($1*100/$2)
  str+=$a
  str+="%"
  echo -e -n $str
}

#generates $1 random files with random text, of size between $2 and $3, in folder $4
chars="abcdefghijklmnopqrstuvwxyz1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ.,?/+=-*/;:[]{}()@"
RAND32="tr -dc '[:alnum:]' < /dev/urandom | dd bs=8 count=4 2>/dev/null"
let len=$3-$2
print_state 0 $1
for ((f = 0; f < $1; f++)); do
  str=""
  RAND=$(od -vAn -N4 -t u4 < /dev/urandom)
  size=$((($RAND%$len)+$2+1))
  str=$(eval "tr -dc '[:alnum:]' < /dev/urandom | dd bs=16 count=$(($size/16)) 2>/dev/null")
  #for ((i=0; i < $size; i++)); do
  #  str+="${chars:($RANDOM)%${#chars}:1}"
  #done
  #for ((i=0; i < $((($size%32)+1)); i++)); do
    #str+=$(eval $RAND32)
  #done
  echo -n $str > "$4/file_$f.txt"
  print_state $(($f+1)) $1
done
echo ""

