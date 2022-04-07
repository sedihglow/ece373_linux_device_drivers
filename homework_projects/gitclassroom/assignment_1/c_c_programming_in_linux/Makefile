BIN = ./bin/temp_conv.out
CC=gcc
CCFLAGS = -I ./include -I -c -Wall -Wextra -pthread 
CCFLAGS+= -m64 -O0 -g -pedantic -std=c99
LDFLAGS = -pthread -m64

SRCS=$(wildcard ./src/*.c)
OBJS=$(SRCS:.c=.o)

.PHONY: all
all: $(BIN)

$(BIN):$(OBJS)
						$(CC) $(OBJS) -o $@ $(LDFLAGS)
						
%.o:%.c
						$(CC) $(CCFLAGS) -o $@ -c $<

.PHONY: valgrind 
valgrind:				# check for memory leak
						$(info -- Checking For Memory Leaks --)
						make
						valgrind --leak-check=full --show-leak-kinds=all ./bin/temp_conv.out

.PHONY: debug
debug:					# GNU debugger
						$(info -- Debugging --)
						gdb ./bin/temp_conv.out

.PHONY: clean
clean:					# clean the directory
						$(info -- Cleaning The Directory --)
						rm -rf ./src/*.o ./bin/temp_conv.out
.PHONY: run
run:					# run the program as follows
						$(info -- Running Program --)
						make
						./bin/temp_conv.out -v
