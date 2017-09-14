CC = mips-linux-gnu-gcc
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
BIN = rb_test
CFLAGS = -Wall -O2

$(BIN): $(OBJS)
	$(CC) $^ -o $@

.PHONY: clean
clean:
	$(RM) $(BIN) $(OBJS)
