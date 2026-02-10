CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2

# automatisch alle .c bestanden pakken
SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.o)

TARGET = polshell

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f *.o $(TARGET)

.PHONY: clean run
