#!bin/bash

SERVERPID = $!  #get the server PID ($! is the last background process PID)
CLIENT = ./build/exec/client.o
SOCKFILE = ./build/exec/socket.sk

rm -r ./tests/test1/return
mkdir ./tests/test1/return
$CLIENT -f $SOCKFILE -t 200 -p -w ./tests/test1/writeall -D ./tests/test1/return
$CLIENT -f $SOCKFILE -t 200 -p -w ./tests/test1/writeall_part,1 -D ./tests/test1/return
$CLIENT -f $SOCKFILE -t 200 -p -W ./tests/test1/file1.txt,./tests/test1/file2.txt -u ./tests/test1/file1.txt,./tests/test1/file2.txt -D ./tests/test1/return
$CLIENT -f $SOCKFILE -t 200 -p -r ./tests/test1/file1.txt,./tests/test1/file2.txt -d ./tests/test1/return
$CLIENT -f $SOCKFILE -t 200 -p -R -d ./tests/test1/return
$CLIENT -f $SOCKFILE -t 200 -p -l ./tests/test1/file1.txt,./tests/test1/file2.txt -c ./tests/test1/file1.txt,./tests/test1/file2.txt

kill $SERVERPID -s SIGHUP 