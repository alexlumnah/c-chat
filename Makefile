# Define the compiled
CC = gcc

# Compiler Flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS = -g -Wall -Wpedantic -Wextra -fsanitize=address,undefined,signed-integer-overflow
LDFLAGS = 

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

all: client server

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

#main: $(OBJ)
#	$(CC) -o main $^ $(CFLAGS) $(LDFLAGS)

client: src/client.o src/sock.o
	$(CC) -o client $^ $(CFLAGS) $(LDFLAGS)

server: src/server.o src/sock.o
	$(CC) -o server $^ $(CFLAGS) $(LDFLAGS)

clean:
	rm server client $(OBJ)
