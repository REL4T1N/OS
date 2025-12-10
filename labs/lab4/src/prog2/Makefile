CC = gcc
CFLAGS = -Wall -Wextra -I../../include
LDFLAGS = -ldl -Wl,-rpath,../../build/lib
RM = rm -f

TARGET = ../../build/program2
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	$(RM) $(TARGET)

.PHONY: all clean