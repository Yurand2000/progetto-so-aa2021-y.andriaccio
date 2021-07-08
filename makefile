CC		= gcc
CFLAGS		= -Wall -pedantic -std=gnu11
CFLAGS_END	= -lpthread
CRELFLAGS	= -O3
CDEBFLAGS	= -g -I ./src/source

SRCFLD		= ./src
SOURCE		= $(SRCFLD)/source
SEXEC		= $(SRCFLD)/exec
BUILD		= ./build
BLDSRC		= $(BUILD)/source
BLDEXE		= $(BUILD)/exec
TESTD		= $(SRCFLD)/tests

#recursive wildcard, thanks to larskholte on StackOverflow.com, question 2483182.
define rwildcard =
$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))
endef

SRC_FILE = $(call rwildcard, $(SOURCE), *.c)
OBJ_FILE = $(patsubst $(SOURCE)/%.c, $(BLDSRC)/%.o, $(SRC_FILE))
SRC_EXEC = $(call rwildcard, $(SEXEC), *.c)
OUT_EXEC = $(patsubst $(SEXEC)/%.c, $(BLDEXE)/%.out, $(SRC_TEST))

.PHONY:	all help debug clean clear cleanobj cleanall
.PHONY: server server_release server_debug client client_release client_debug
.PHONY: test1 test2 test2lru test2lfu test3 dummy_clear stats

TARGET	= server_release client_release
TARGET_DEBUG	= server_debug client_debug

all	: $(TARGET)

debug	: $(TARGET_DEBUG)

server_debug	: CFLAGS += $(CDEBFLAGS)
server_debug	: server
server_release	: CFLAGS += $(CRELFLAGS)
server_release	: server
server		: $(BLDEXE)/server.out
$(BLDEXE)/server.out	: $(BLDEXE)/server.o $(BLDSRC)/minilzo.o $(OBJ_FILE)
	$(CC) $(CFLAGS) $^ -o $@ $(CFLAGS_END)

client_debug	: CFLAGS += $(CDEBFLAGS)
client_debug	: client
client_release	: CFLAGS += $(CRELFLAGS)
client_release	: client
client		: $(BLDEXE)/client.out
$(BLDEXE)/client.out	: $(BLDEXE)/client.o $(BLDSRC)/minilzo.o $(OBJ_FILE)
	$(CC) $(CFLAGS) $^ -o $@ $(CFLAGS_END)

help :
	@echo "* available targets: ---------------------------------"
	@echo "help all debug clean clear cleanobj cleanall"
	@echo "test1 test2 test3 stats dummy_clear"
	@echo "* commands: ------------------------------------------"
	@echo "| help         - shows this help screen"
	@echo "| all          - compiles for release"
	@echo "| debug        - complies for debug"
	@echo "------------------------------------------------------"
	@echo "| clean, clear - deletes executables and obj files"
	@echo "| cleanobj     - deletes obj files only"
	@echo "| dummy_clear  - delets the files needed for test3"
	@echo "| cleanall     - calls [clean] and [dummy_clear]. also"
	@echo "                 deletes the default log file."
	@echo "------------------------------------------------------"
	@echo "| test1, test2, test3 - run default tests"
	@echo "| test2lru, test2lfu  - run test2 with different cache"
	@echo "                        cache miss algorithms"
	@echo "| stats               - run the statistiche.sh script "
	@echo "                        on the default log file. (This"
	@echo "                        is also  the default  log  for"
	@echo "                        all the tests.)"

test1	: all
	@$(TESTD)/test1/test1.sh $(BLDEXE)
test2	: all
	@$(TESTD)/test2/test2.sh $(BLDEXE) fifo
test2lru : all
	@$(TESTD)/test2/test2.sh $(BLDEXE) lru
test2lfu : all
	@$(TESTD)/test2/test2.sh $(BLDEXE) lfu
test3	: all dummy
	@$(TESTD)/test3/test3.sh $(BLDEXE)
stats	:
	@$(SEXEC)/statistiche.sh log.txt

dummy	:
	@echo "creating files for test3... (circa 64Mb)"
	@echo "the files are created only once."
	@echo "if you want to regenerate the files, run 'make dummy_clear'"
	@echo "and then restart the test3."
	@-mkdir -p $(TESTD)/test3/files
	@$(SEXEC)/dummy_gen.sh 200 275000 450000 $(TESTD)/test3/files/
	@echo "dummy" > dummy

dummy_clear :
	@-rm -f dummy
	@-rm -f -r $(TESTD)/test3/files/
	@-mkdir -p $(TESTD)/test3/files

#CLEAR FUNCTIONS
clear : clean
clean :
	@-rm -f -r $(BUILD)
	@-rm -f -r $(COVERAGE)	

cleanobj :
	@-rm -f -r $(BUILD)/exec/*.o
	@-rm -f -r $(BUILD)/source

cleanall : clean dummy_clear
	@-rm -f log.txt

#rule template for obj compilation
obj_from_src = $(subst $(SRCFLD),$(BUILD),$(subst .c,.o,$(1)))

#rule generation
define comp_o = 
source	 := $(1)
target	 := $(2)
add_args := $(3)
$$(target) : $$(source) $$(add_args)
	@mkdir -p $$(dir $$@)
	$(CC) $$(CFLAGS) -c $$< -o $$@ $$(CFLAGS_END)
endef

define make_obj_rules =
$(foreach i,$(1),$(eval $(call comp_o,$(i),$(call obj_from_src,$(i)),$(wildcard $(SRCFLD)/*.h))))
endef

$(call make_obj_rules,$(SRC_FILE))
$(call make_obj_rules,$(SRC_EXEC))

#minilzo compression library
$(BLDSRC)/minilzo.o : $(SRCFLD)/minilzo/minilzo.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@ $(CFLAGS_END)
