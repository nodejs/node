CFLAGS  := -std=c99 -Wall -O2 -pthread
LDFLAGS := -pthread
LIBS    := -lm
ifeq ($(shell uname),SunOS)
LIBS    += -lnsl -lsocket -lresolv
endif

SRC  := wrk.c aprintf.c stats.c units.c ae.c zmalloc.c http_parser.c tinymt64.c
BIN  := wrk

ODIR := obj
OBJ  := $(patsubst %.c,$(ODIR)/%.o,$(SRC))

all: $(BIN)

clean:
	$(RM) $(BIN) obj/*

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJ): config.h Makefile | $(ODIR)

$(ODIR):
	@mkdir $@

$(ODIR)/%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: all clean
.SUFFIXES:
.SUFFIXES: .c .o

vpath %.c src
vpath %.h src
