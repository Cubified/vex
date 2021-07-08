all: vex

CC=gcc

LIBS=
CFLAGS=-O3 -pipe -s -pedantic
DEBUGCFLAGS=-Og -pipe -g -Wall -Wextra

INPUT=vex.c
OUTPUT=vex

RM=/bin/rm

.PHONY: vex
vex:
	$(CC) $(INPUT) -o $(OUTPUT) $(LIBS) $(CFLAGS)

debug:
	$(CC) $(INPUT) -o $(OUTPUT) $(LIBS) $(DEBUGCFLAGS)

clean:
	if [ -e $(OUTPUT) ]; then $(RM) $(OUTPUT); fi
