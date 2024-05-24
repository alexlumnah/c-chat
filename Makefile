# Define the compiled
CC = gcc

# Compiler Flags:
CFLAGS = -g -Wall -Wpedantic -Wextra -fsanitize=address,undefined,signed-integer-overflow
LDFLAGS = -lncurses

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

TEST_SRC = $(wildcard test/*.c)
TEST_OBJ = $(TEST_SRC:.c=.o)

all: main

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

main: $(OBJ)
	$(CC) -o chat $^ $(CFLAGS) $(LDFLAGS)

test: test/test.o src/sock.o src/serial.o
	$(CC) -o run_test $^ $(CFLAGS) $(LDFLAGS)

clean:
	rm chat run_test test/test.o $(OBJ)

tidy:
	clang-tidy src/* --

cppcheck:
	cppcheck --enable=portability --check-level=exhaustive --enable=style src/*.c
