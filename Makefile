EVDIR=$(HOME)/local/libev
V8INC = $(HOME)/src/v8/include
V8LIB = $(HOME)/src/v8/libv8.a

CFLAGS = -g -I$(V8INC) -Ideps/oi -DHAVE_GNUTLS=0 -Ideps/ebb 
LDFLAGS = -lev #-lefence

ifdef EVDIR
	CFLAGS += -I$(EVDIR)/include
	LDFLAGS += -L$(EVDIR)/lib
endif

server: server.o oi_socket.o ebb_request_parser.o
	g++ $(LDFLAGS) $(V8LIB) $^ -o server 

server.o: server.cc 
	g++ $(CFLAGS) -c $<

ebb_request_parser.o: ebb_request_parser.c deps/ebb/ebb_request_parser.h 
	gcc $(CFLAGS) -c $<

ebb_request_parser.c: deps/ebb/ebb_request_parser.rl
	ragel -s -G2 $< -o $@

oi_socket.o: deps/oi/oi_socket.c deps/oi/oi_socket.h 
	gcc $(CFLAGS) -c $<

clean:
	rm -f ebb_request_parser.c
	rm -f *.o 
	rm -f server

.PHONY: clean test
