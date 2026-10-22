CC = gcc
CFLAGS = -w -march=rv64gcv -pthread -ggdb -O0 -fstack-protector-all

TARGET = ludtm_crash
SRC = ludtm.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $< -o $@

test: $(TARGET)
	./$(TARGET)

.PHONY: all test clean

clean:
	rm -f $(TARGET)
