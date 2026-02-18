CC := gcc
FLAGS := -Wall
MAINS := src/main.c src/gen.c
CFILES := $(wildcard src/*.c)
HFILES := $(patsubst %.c, %.h, $(filter-out $(MAINS), $(CFILES)))
LIBS := $(patsubst src/%.h, obj/%.o, $(HFILES))

ifdef DEBUG
	FLAGS += -ggdb -O0
endif

all: objFolder main

objFolder:
	mkdir -p obj

obj/%.o: src/%.c $(HFILES)
	$(CC) $< -o $@ $(FLAGS) -c

main: obj/main.o $(LIBS)
	$(CC) $^ -o $@ $(FLAGS)

clean:
	rm -rf main obj

format: src/*.c src/*.h
	@ clang-format -i src/*.c src/*.h -style=file

.PHONY: all objFolder main clean format
