EVDIR=$(HOME)/local/libev
V8INC = $(HOME)/src/v8/include
V8LIB = $(HOME)/src/v8/libv8_g.a

CFLAGS = -g -I$(V8INC) -Ideps/oi -DHAVE_GNUTLS=0 -Ideps/ebb 
LDFLAGS = -lev -pthread # -lefence

ifdef EVDIR
	CFLAGS += -I$(EVDIR)/include
	LDFLAGS += -L$(EVDIR)/lib
endif

node: node.o node_tcp.o node_http.o oi_socket.o oi_async.o oi_buf.o ebb_request_parser.o
	g++ -o node $^ $(LDFLAGS) $(V8LIB) 

node.o: node.cc 
	g++ $(CFLAGS) -c $<

node_tcp.o: node_tcp.cc 
	g++ $(CFLAGS) -c $<

node_http.o: node_http.cc 
	g++ $(CFLAGS) -c $<

oi_socket.o: deps/oi/oi_socket.c deps/oi/oi_socket.h 
	gcc $(CFLAGS) -c $<

oi_async.o: deps/oi/oi_async.c deps/oi/oi_async.h 
	gcc $(CFLAGS) -c $<

oi_buf.o: deps/oi/oi_buf.c deps/oi/oi_buf.h 
	gcc $(CFLAGS) -c $<
	
ebb_request_parser.o: ebb_request_parser.c deps/ebb/ebb_request_parser.h 
	g++ $(CFLAGS) -c $<

ebb_request_parser.c: deps/ebb/ebb_request_parser.rl
	ragel -s -G2 $< -o $@

clean:
	rm -f ebb_request_parser.c
	rm -f *.o 
	rm -f node

.PHONY: clean test
