#ifndef tcp_h
#define tcp_h

#include <v8.h>
#include <ev.h>

using namespace v8;

Handle<Object> tcp_initialize (struct ev_loop *);

#endif
