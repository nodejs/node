CC=cc
CFLAGS= -g -I../../include
LIBS= -L../.. ../../libssl.a ../../libcrypto.a -ldl
EXAMPLES=saccept sconnect client-arg client-conf

all: $(EXAMPLES) 

saccept: saccept.o
	$(CC) -o saccept saccept.o $(LIBS)

sconnect: sconnect.o
	$(CC) -o sconnect sconnect.o $(LIBS)

client-arg: client-arg.o
	$(CC) -o client-arg client-arg.o $(LIBS)

client-conf: client-conf.o
	$(CC) -o client-conf client-conf.o $(LIBS)

clean:	
	rm -f $(EXAMPLES) *.o

