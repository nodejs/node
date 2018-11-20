CC=cc
CFLAGS=-g

untgz: untgz.o ../../libz.a
	$(CC) $(CFLAGS) -o untgz untgz.o -L../.. -lz

untgz.o: untgz.c ../../zlib.h
	$(CC) $(CFLAGS) -c -I../.. untgz.c

../../libz.a:
	cd ../..; ./configure; make

clean:
	rm -f untgz untgz.o *~
