CC=cc
CFLAGS= -g -I../../include
#LIBS=  -L../.. -lcrypto -lssl
LIBS= -L../.. ../../libssl.a ../../libcrypto.a

# the file conn.c requires a file "proxy.h" which I couldn't find...
#EXAMPLES=base64 conn loadrsa
EXAMPLES=base64 loadrsa

all: $(EXAMPLES) 

base64: base64.o
	$(CC) -o base64 base64.o $(LIBS)
#
# sorry... can't find "proxy.h"
#conn: conn.o
#	$(CC) -o conn conn.o $(LIBS)

loadrsa: loadrsa.o
	$(CC) -o loadrsa loadrsa.o $(LIBS)

clean:	
	rm -f $(EXAMPLES) *.o

