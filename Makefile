CC = gcc
CFLAGS = -std=c99 -pedantic -Wall -Os
SRCDIR = src
SRC = $(SRCDIR)/microtar.c
OBJ = $(SRC:.c=.o)
SOBJ = $(SRC:.c=.so)

$(SOBJ): $(OBJ)
	$(CC) -shared $(OBJ) -o $(SOBJ)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC) -o $(OBJ)

.PHONY: clean
clean:
	rm -rf $(OBJ) $(SOBJ)
