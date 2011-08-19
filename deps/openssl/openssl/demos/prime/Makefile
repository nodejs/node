CC=cc
CFLAGS= -g -I../../include -Wall
LIBS=  -L../.. -lcrypto
EXAMPLES=prime

all: $(EXAMPLES) 

prime: prime.o
	$(CC) -o prime prime.o $(LIBS)

clean:	
	rm -f $(EXAMPLES) *.o

test: all
	@echo Test creating a 128-bit prime
	./prime 128
	@echo Test creating a 256-bit prime
	./prime 256
	@echo Test creating a 512-bit prime
	./prime 512
