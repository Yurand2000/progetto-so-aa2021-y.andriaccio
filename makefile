CC	=	gcc
CFLAGS	=	-Wall
CFLAGS_END	=	-lpthread
CRELFLAGS	=	-O3
CDEBFLAGS	=	-g -I ./src/source # -fprofile-arcs -ftest-coverage #(gcov options)

SRCFLD	=	./src
SOURCE	=	$(SRCFLD)/source
SEXEC	=	$(SRCFLD)/exec
TESTD	=	$(SRCFLD)/tests
BUILD	=	./build
BLDSRC	=	$(BUILD)/source
BLDEXE	=	$(BUILD)/exec
BLDTEST	=	$(BUILD)/tests
COVERAGE	=	./coverage

#recursive wildcard, thanks to larskholte on StackOverflow.com, question 2483182.
define	rwildcard	=
$(foreach d, $(wildcard $(1:=/*)),
	$(call rwildcard, $d, $2) $(filter $(subst *,%,$2), $d)
)
endef

SRC_FILE	=	$(call rwildcard, $(SOURCE), *.c)
OBJ_FILE	=	$(patsubst $(SOURCE)/%.c, $(BLDSRC)/%.o, $(SRC_FILE))
SRC_TEST	=	$(call rwildcard, $(TESTD), *.c)
OBJ_TEST	=	$(patsubst $(TESTD)/%.c, $(BLDTEST)/%.o, $(SRC_TEST))
OUT_TEST	=	$(patsubst %.o, %.out, $(OBJ_TEST))
SRC_EXEC	=	$(call rwildcard, $(SEXEC), *.c)
OUT_EXEC	=	$(patsubst $(SEXEC)/%.c, $(BLDEXE)/%.out, $(SRC_TEST))

.PHONY:	all help debug clean clear cleanobj #cleangcov #(gcov options)
.PHONY: server server_release server_debug client client_release client_debug
.PHONY: test1 test2 test3
#.PHONY: utest utest_dirs 

help	:
	@echo "available targets:"
	@echo "help all debug clean/clear cleanobj"
	@echo "server server_release server_debug"
	@echo "client client_release client_debug"
	@echo "test1 test2 test3"

TARGET	= server_release client_release
TARGET_DEBUG	= server_debug client_debug

all	: $(TARGET)

debug	: $(TARGET_DEBUG)

server	: $(BLDEXE)/server.out
$(BLDEXE)/server.out	: $(BLDEXE)/server.o $(OBJ_FILE)
	$(CC) $(CFLAGS) $^ -o $@ $(CFLAGS_END)
server_debug	: CFLAGS += $(CDEBFLAGS)
server_debug	: server
server_release	: CFLAGS += $(CRELFLAGS)
server_release	: server

client	: $(BLDEXE)/client.out
$(BLDEXE)/client.out	: $(BLDEXE)/client.o $(OBJ_FILE)
	$(CC) $(CFLAGS) $^ -o $@ $(CFLAGS_END)
client_debug	: CFLAGS += $(CDEBFLAGS)
client_debug	: client
client_release	: CFLAGS += $(CRELFLAGS)
client_release	: client

test1	: all
	@$(TESTD)/test1/test1.sh $(BLDEXE)
test2	: all
	@$(TESTD)/test2/test2.sh $(BLDEXE)
test3	: all
	@$(TESTD)/test3/test3.sh $(BLDEXE)

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
define comp_o	= 
source	:= $(1)
target	:= $(2)
add_args	:= $(3)
$$(target)	: $$(source) $$(add_args)
	@mkdir -p $$(dir $$@)
	$(CC) $$(CFLAGS) -c $$< -o $$@ $$(CFLAGS_END)
endef

define comp_out	=
source	:= $(1)
target	:= $(2)
objs	:= $(3)
$$(target) : $$(source) $$(objs)
	@mkdir -p $$(dir $$@)
	$(CC) $$(CFLAGS) $$^ -o $$@ $$(CFLAGS_END)
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
