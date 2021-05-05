CC	=	gcc
CFLAGS	=	-Wall

.PHONY: clean all internal

all	:

internal	:	build/tests/databuf_test.out
	echo
	./build/tests/databuf_test.out

clean	:
	-rm -f -r build
	mkdir build
	mkdir build/tests

build/databuf.o	:	source/databuf.c
	$(CC) $(CFLAGS) -c $^ -o $@

build/tests/databuf_test.o	:	tests/databuf_test.c
	$(CC) $(CFLAGS) -c $^ -o $@

build/tests/databuf_test.out	:	build/databuf.o build/tests/databuf_test.o
	$(CC) $(CFLAGS) $^ -o $@
