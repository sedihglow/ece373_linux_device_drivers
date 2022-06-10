BIN = ./bin/i82540EM_blink_driver.out
CC=gcc
CCFLAGS = -I ./include -I -c -Wall -Wextra -pthread 
CCFLAGS+= -m64 -O0 -g -pedantic
LDFLAGS = -pthread -m64

SRCS=$(wildcard ./src_with_pci_h/*.c)
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
	./bin/i82540EM_blink_driver.out -s 0000:00:03.0 -n enp0s3 -v

.PHONY: debug
debug:	# GNU debugger
	$(info -- Debugging --)
	sudo gdb ./bin/i82540EM_blink_driver.out -s 0000:00:03.0 -n enp0s3 -v

.PHONY: clean
clean:	# clean the directory
	$(info -- Cleaning The Directory --)
	rm -rf ./src/*.o ./bin/i82540EM_blink_driver.out ./src_with_pci_h/*.o

.PHONY: run
run: # run the program as follows
	$(info -- Running Program --)
	make
	sudo ./bin/i82540EM_blink_driver.out -s 0000:00:03.0 -n enp0s3 -v

