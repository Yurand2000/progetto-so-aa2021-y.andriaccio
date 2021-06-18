#!bin/bash

SERVERPID = $!  #get the server PID, which is probably not the last one...
CLIENT = ./build/exec/client.o
SOCKFILE = ./build/exec/socket.sk

rm -r ./tests/test3/return
mkdir ./tests/test3/return

REQ1 = $CLIENT -f $SOCKFILE -t 0 -w ./tests/test3/writeall -D ./tests/test3/return
REQ2 = $CLIENT -f $SOCKFILE -t 0 -r ./tests/test3/file1.txt,./tests/test3/file2.txt -d ./tests/test3/return
REQ3 = $CLIENT -f $SOCKFILE -t 0 -R n=5 -d ./tests/test3/return
REQ4 = $CLIENT -f $SOCKFILE -t 0 -W ./tests/test3/file1.txt,./tests/test3/file2.txt
REQ5 = $CLIENT -f $SOCKFILE -t 0 -l ./tests/test3/file1.txt,./tests/test3/file2.txt -c ./tests/test3/file1.txt,./tests/test3/file2.txt
CURR = 0



kill $SERVERPID -s SIGINT