CC	=	gcc
CFLAGS	=	-Wall

SOURCE	= ./source
SEXEC   = $(SOURCE)/exec
BUILD	= ./build
BTEST	= $(BUILD)/tests
BEXEC   = $(BUILD)/exec
TESTD	= ./tests
TESTDATAD = ./tests_data

.PHONY: clean clear cleanobj all internal internal_build server server_debug client client_debug
TARGET = server client

SRC_FILE = $(wildcard $(SOURCE)/*.c)
OBJ_FILE = $(patsubst $(SOURCE)/%.c, $(BUILD)/%.o, $(SRC_FILE))
SRC_TEST = $(wildcard $(TESTD)/*.c)
OUT_TEST = $(patsubst $(TESTD)/%.c, $(BTEST)/%.out, $(SRC_TEST))

all	: $(TARGET)

server :
client_debug : CFLAGS += -g
client_debug : client
client : $(BEXEC)/client.o $(OBJ_FILE)
	$(CC) $(CFLAGS) $^ -o $(BEXEC)/$@.out

internal_build	:	CFLAGS += -g
internal_build	:	$(OBJ_FILE) $(OUT_TEST)
	cp -r -u $(TESTDATAD)/. $(BTEST)	

internal	:	internal_build
	echo
	$(BTEST)/databuf_test.out
	$(BTEST)/net_msg_test.out

clear : clean
clean :
	-rm -f -r $(BUILD)
	mkdir $(BUILD)
	mkdir $(BTEST)
	mkdir $(BEXEC)

cleanobj :
	-rm -r $(BUILD)/*.o

$(BUILD)/%.o : $(SOURCE)/%.c #$(SOURCE)/%.h
	$(CC) $(CFLAGS) -c $< -o $@

#TESTS HERE
$(BTEST)/%.o : $(TESTD)/%.c $(SOURCE)/*.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BTEST)/%.out : $(BTEST)/%.o $(OBJ_FILE)
	$(CC) $(CFLAGS) $^ -o $@

