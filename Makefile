# Define the compiled
CC = gcc

# Compiler Flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
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
	$(CC) -o main $^ $(CFLAGS) $(LDFLAGS)

test: test/test.o src/sock.o src/serialize.o
	$(CC) -o run_test $^ $(CFLAGS) $(LDFLAGS)

clean:
	rm server client $(OBJ)
