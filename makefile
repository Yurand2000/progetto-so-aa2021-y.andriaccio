CC	=	gcc
CFLAGS	=	-Wall
CRELFLAGS =	-O3
CDEBFLAGS =	-g -fprofile-arcs -ftest-coverage -I ./src/source

SRCFLD 	= ./src
SOURCE	= $(SRCFLD)/source
SEXEC   = $(SRCFLD)/exec
TESTD	= $(SRCFLD)/tests
BUILD	= ./build
BLDSRC	= $(BUILD)/source
BLDEXE	= $(BUILD)/exec
BLDTEST = $(BUILD)/tests
COVERAGE = ./coverage

#recursive wildcard, thanks to larskholte on StackOverflow.com, question 2483182.
define rwildcard =
$(foreach d, $(wildcard $(1:=/*)),
	$(call rwildcard, $d, $2) $(filter $(subst *,%,$2), $d)
)
endef

SRC_FILE = $(call rwildcard, $(SOURCE), *.c)
OBJ_FILE = $(patsubst $(SOURCE)/%.c, $(BLDSRC)/%.o, $(SRC_FILE))
SRC_TEST = $(call rwildcard, $(TESTD), *.c)
OBJ_TEST = $(patsubst $(TESTD)/%.c, $(BLDTEST)/%.o, $(SRC_TEST))
OUT_TEST = $(patsubst %.o, %.out, $(OBJ_TEST))
SRC_EXEC = $(call rwildcard, $(SEXEC), *.c)
OUT_EXEC = $(patsubst $(SEXEC)/%.c, $(BLDEXE)/%.out, $(SRC_TEST))

.PHONY: all help debug clean clear cleanobj cleangcov
.PHONY: server server_debug client client_debug
.PHONY: utest utest_dirs test1 test2 test3

help	:
	@echo "available targets:"
	@echo "all help debug clean/clear cleanobj cleangcov"
	@echo "server server_debug client client_debug"
	@echo "utest utest_run"

TARGET = server client
TARGET_DEBUG = server_debug client_debug

all	: $(TARGET)

debug	: $(TARGET_DEBUG)

server	:
server_debug	:

client	: $(BLDEXE)/client.o $(OBJ_FILE)
	$(CC) $(CFLAGS) $^ -o $(BLDEXE)/$@.out
client_debug : CFLAGS += $(CDEBFLAGS)
client_debug : client

#test1 : server client
#  @valgrind --leak-check=full $(BEXEC)/server.out -c ./tests/test1/test1.cfg &
#  @./tests/test1/client.sh
#  @fg
#test2 : server client
#  @$(BEXEC)/server.out -c ./tests/test2/test2.cfg &
#  @./tests/test2/client.sh
#  @fg
#test3 : server client
#  @$(BEXEC)/server.out -c ./tests/test3/test3.cfg &
#  @./tests/test3/client.sh
#  @fg

#UNIT TESTS
utest_dirs :
	@mkdir -p $(dir $(OBJ_FILE)) $(dir $(OUT_TEST))

utest	: CFLAGS += $(CDEBFLAGS)
utest	: utest_dirs
utest	: $(OBJ_FILE) $(OBJ_TEST) $(OUT_TEST)

utest_run : utest
	@echo "* * * Starting unit tests; output saved to tests.txt"
	@-rm tests.txt
	@$(patsubst %,% >> tests.txt 2>> tests.txt;,$(OUT_TEST))
	@echo "* * * Creating coverage files for unit tests."
	@$(foreach i,$(SRC_FILE),$(call gcov_run,$(i)))
	@echo "* * * Done! You can look at the coverage files into the ./coverage folder."
	@echo "      The folder has the same structure as the src folder."

gcov_run = mkdir -p $(call gcov_out_dir,$(1)); rm $(call gcov_out_file,$(1)); gcov $(1) -o $(call obj_from_src,$(1)) -t >> $(call gcov_out_file,$(1));
gcov_out_file = $(subst $(SRCFLD),$(COVERAGE),$(subst .c,.gcov,$(1)))
gcov_out_dir = $(dir $(call gcov_out_file,$(1)))

#CLEAR FUNCTIONS
clear : clean
clean :
	@-rm -f -r $(BUILD)
	@-rm -f -r $(COVERAGE)	

cleanobj :
	@-rm -r $(BUILD)/*/*.o

cleangcov :
	@-rm -r $(BUILD)/*/*.gcda
	@-rm -r $(BUILD)/*/*.gcno
	@-rm -f -r $(COVERAGE)

#rule template for obj compilation
obj_from_src = $(subst $(SRCFLD),$(BUILD),$(subst .c,.o,$(1)))
out_from_obj = $(subst .o,.out,$(1))

#rule generation
define comp_o = 
source := $(1)
target := $(2)
add_args := $(3)
$$(target) : $$(source) $$(add_args)
	@mkdir -p $$(dir $$@)
	$(CC) $$(CFLAGS) -c $$< -o $$@
endef

define comp_out =
source := $(1)
target := $(2)
objs := $(3)
$$(target) : $$(source) $$(objs)
	@mkdir -p $$(dir $$@)
	$(CC) $$(CFLAGS) $$^ -o $$@
endef

define make_obj_rules =
$(foreach i,$(1),$(eval $(call comp_o,$(i),$(call obj_from_src,$(i)),$(wildcard $(SRCFLD)/*.h))))
endef

$(call make_obj_rules,$(SRC_FILE))
$(call make_obj_rules,$(SRC_TEST))
$(call make_obj_rules,$(SRC_EXEC))

define make_out_rules =
$(foreach i,$(1),$(eval $(call comp_out,$(i),$(call out_from_obj,$(i)),$(OBJ_FILE))))
endef

$(call make_out_rules,$(OBJ_TEST))
