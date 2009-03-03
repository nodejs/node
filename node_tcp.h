#ifndef node_tcp_h
#define node_tcp_h

#include <v8.h>
#include <ev.h>

using namespace v8;

Handle<Object> node_tcp_initialize (struct ev_loop *);

#endif
