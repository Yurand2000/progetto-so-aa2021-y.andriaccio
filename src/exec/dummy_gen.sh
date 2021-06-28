#!/bin/bash

#generates $1 random files with random text, of size between $2 and $3, in folder $4
chars="abcdefghijklmnopqrstuvwxyz1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ.,?/+=-*/;:[]{}()@"
len=$3-$2
elems={1..$1}
for ((f = 0; f < $1; f++)); do
  size=($RANDOM%$len)+$2+1
  echo -n > "$4/file_$f.txt"
  for ((i=$2; i < $size; i++)); do
    echo -n "${chars:($RANDOM)%${#chars}:1}" >> "$4/file_$f.txt"
  done
done
