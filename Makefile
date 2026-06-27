CC 		:= gcc
FLAGS 	:= -Wall -Wextra -MMD -MP
SRC 	:= $(wildcard src/**/*.c)
OBJ 	:= $(SRC:src/%.c=obj/%.o)
TESTS	:= $(SRC:src/test/%.c=%)
DEP 	:= $(OBJ:.o=.d)

ifdef DEBUG
	FLAGS += -g -O0
endif

all: main tests

obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $< -o $@ -c $(FLAGS)

test%: obj/test/test%.o obj/btree.o
	$(CC) $^ -o $@ $(FLAGS)

main: obj/main.o obj/btree.o
	$(CC) $^ -o $@ $(FLAGS)

tests: $(TESTS)

clean:
	rm -rf obj main $(TESTS)

-include $(DEP)

.PHONY: all clean tests

.PRECIOUS: obj/%.o
