CC	=	gcc
CFLAGS	=	-Wall

SOURCE	= ./source
BUILD	= ./build
BTEST	= $(BUILD)/tests
TESTD	= ./tests

.PHONY: clean cleanobj all internal set_debug server client
TARGET = server client

SRC_FILE = $(wildcard $(SOURCE)/*.c)
OBJ_FILE = $(patsubst $(SOURCE)/%.c, $(BUILD)/%.o, $(SRC_FILE))
SRC_TEST = $(wildcard $(TESTD)/*.c)
OUT_TEST = $(patsubst $(TESTD)/%.c, $(BTEST)/%.out, $(SRC_TEST))

all	: $(TARGET)

server :
client :

internal_build	:	CFLAGS += -g
internal_build	:	$(OBJ_FILE) $(OUT_TEST)

internal	:	internal_build
	echo
	$(BTEST)/databuf_test.out
	$(BTEST)/net_msg_test.out

clear : clean
clean :
	-rm -f -r $(BUILD)
	mkdir $(BUILD)
	mkdir $(BTEST)

cleanobj :
	-rm -r $(BUILD)/*.o

$(BUILD)/%.o : $(SOURCE)/%.c #$(SOURCE)/%.h
	$(CC) $(CFLAGS) -c $< -o $@

#TESTS HERE
$(BTEST)/%.o : $(TESTD)/%.c $(SOURCE)/*.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BTEST)/%.out : $(BTEST)/%.o $(OBJ_FILE)
	$(CC) $(CFLAGS) $^ -o $@

