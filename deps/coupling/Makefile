# CFLAGS and LDFLAGS are for the users to override from the command line.
CFLAGS	= -g -I. -Wall -Werror -Wextra #-DNDEBUG=1
LDFLAGS	= -pthread

CC = gcc
AR = ar
RANLIB = ranlib

DEP = coupling.h
SRC = coupling.c
OBJ = ${SRC:.c=.o}

NAME=libcoupling
OUTPUT_A=$(NAME).a

all: $(OUTPUT_A) 

$(OUTPUT_A): $(OBJ)
	$(AR) cru $(OUTPUT_A) $(OBJ)
	$(RANLIB) $(OUTPUT_A)

.c.o:
	$(CC) -c ${CFLAGS} $<

${OBJ}: ${DEP}

test: test.c $(OUTPUT_A)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ test.c $(OUTPUT_A)

clean:
	rm -f $(OUTPUT_A) *.o test

.PHONY: all clean 
