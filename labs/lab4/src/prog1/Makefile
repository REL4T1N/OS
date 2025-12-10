CC = gcc
CFLAGS = -Wall -Wextra -I../../include
LDFLAGS = -L../../build/lib -l1 -lm -Wl,-rpath='$$ORIGIN/lib'
RM = rm -f

TARGET = ../../build/program1
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	$(RM) $(TARGET)

.PHONY: all clean