BIN = ./bin/i82540EM_blink_driver.out
CC=gcc
CCFLAGS = -I ./include -I -c -Wall -Wextra -pthread
CCFLAGS+= -m64 -O0 -g -pedantic
LDFLAGS = -pthread -m64 -lpci

SRCS=$(wildcard ./src/*.c)
OBJS=$(SRCS:.c=.o)

.PHONY: all
all: $(BIN)

$(BIN):$(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
						
%.o:%.c
	$(CC) $(CCFLAGS) -o $@ -c $<

.PHONY: valgrind 
valgrind: # check for memory leak
	$(info -- Checking For Memory Leaks --)
	make
	sudo valgrind -s --leak-check=full --show-leak-kinds=all \
	./bin/i82540EM_blink_driver.out -v

.PHONY: debug
debug:	# GNU debugger
	$(info -- Debugging --)
	sudo gdb ./bin/i82540EM_blink_driver.out

.PHONY: clean
clean:	# clean the directory
	$(info -- Cleaning The Directory --)
	rm -rf ./bin/i82540EM_blink_driver.out ./src/*.o

.PHONY: run
run: # run the program as follows
	$(info -- Running Program --)
	make
	sudo ./bin/i82540EM_blink_driver.out -v

