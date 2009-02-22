EVDIR=$(HOME)/local/libev
V8INC = $(HOME)/src/v8/include
V8LIB = $(HOME)/src/v8/libv8.a

CFLAGS = -g -I$(V8INC) -Ideps/oi -DHAVE_GNUTLS=0 -Ideps/ebb 
LDFLAGS = -lev -pthread #-lefence

ifdef EVDIR
	CFLAGS += -I$(EVDIR)/include
	LDFLAGS += -L$(EVDIR)/lib
endif

server: js_http_request_processor.o server.o oi_socket.o ebb_request_parser.o oi_buf.o
	g++ -o server $^ $(LDFLAGS) $(V8LIB) 

server.o: server.cc 
	g++ $(CFLAGS) -c $<
	
js_http_request_processor.o: js_http_request_processor.cc 
	g++ $(CFLAGS) -c $<

ebb_request_parser.o: ebb_request_parser.c deps/ebb/ebb_request_parser.h 
	g++ $(CFLAGS) -c $<

ebb_request_parser.c: deps/ebb/ebb_request_parser.rl
	ragel -s -G2 $< -o $@

oi_socket.o: deps/oi/oi_socket.c deps/oi/oi_socket.h 
	gcc $(CFLAGS) -c $<

oi_buf.o: deps/oi/oi_buf.c deps/oi/oi_buf.h 
	gcc $(CFLAGS) -c $<

clean:
	rm -f ebb_request_parser.c
	rm -f *.o 
	rm -f server

.PHONY: clean test
