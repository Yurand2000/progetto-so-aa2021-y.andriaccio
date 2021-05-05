CC	=	gcc
CFLAGS	=	-Wall

.PHONY: clean all internal set_debug

all	:

internal_build	:	CFLAGS += -g
internal_build	:	build/tests/databuf_test.out build/tests/net_msg_test.out

internal	:	internal_build
	echo
	./build/tests/databuf_test.out
	./build/tests/net_msg_test.out

clean	:
	-rm -f -r build
	mkdir build
	mkdir build/tests

build/databuf.o	:	source/databuf.c source/databuf.h
	$(CC) $(CFLAGS) -c $< -o $@

build/net_msg.o	:	source/net_msg.c source/net_msg.h source/hash.h
	$(CC) $(CFLAGS) -c $< -o $@

#TESTS HERE
build/tests/databuf_test.o	:	tests/databuf_test.c
	$(CC) $(CFLAGS) -c $^ -o $@

build/tests/databuf_test.out	:	build/databuf.o build/tests/databuf_test.o
	$(CC) $(CFLAGS) $^ -o $@

build/tests/net_msg_test.o	:	tests/net_msg_test.c
	$(CC) $(CFLAGS) -c $^ -o $@

build/tests/net_msg_test.out	:	build/databuf.o build/net_msg.o build/tests/net_msg_test.o
	$(CC) $(CFLAGS) $^ -o $@
