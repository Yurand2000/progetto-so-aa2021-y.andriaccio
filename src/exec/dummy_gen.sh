#!bin/bash

#generates $1 random files with random text, of size between $2 and $3, in folder $4
chars="abcdefghijklmnopqrstuvwxyz1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ.,?/+=-*/;:[]{}()@"
len=$3-$2
for f in {1..$1} ; do
  size = ($RANDOM % $len) + $2
  echo -n > "$4/file_$f.txt"
  for i in {$2..$size} ; do
    echo -n "${chars:($RANDOM)%${#chars}:1}" >> "$4/file_$f.txt"
  done
done