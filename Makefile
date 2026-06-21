CC 		:= gcc
FLAGS 	:= -Wall -Wextra -MMD -MP
SRC 	:= $(wildcard src/*.c)
TEST	:= $(wildcard test/*.c)
OBJ 	:= $(SRC:src/%.c=obj/%.o) $(TEST:test/%.c=obj/%_test.o)
DEP 	:= $(OBJ:.o=.d)

ifdef DEBUG
	FLAGS += -g -O0
endif

all: main

objFolder:
	mkdir -p obj

obj/%_test.o: test/%.c | objFolder
	$(CC) $< -o $@ -c $(FLAGS)

obj/%.o: src/%.c | objFolder
	$(CC) $< -o $@ -c $(FLAGS)

main: obj/main.o obj/btree.o
	$(CC) $^ -o $@ $(FLAGS)

benchmark1.out: obj/benchmark1_test.o obj/btree.o
	$(CC) $^ -o $@ $(FLAGS)

clean:
	rm -rf obj

-include $(DEP)

.PHONY: all clean objFolder

