#ifndef node_http_h
#define node_http_h

#include <v8.h>
#include <ev.h>

using namespace v8;

Handle<Object> node_http_initialize (struct ev_loop *);

#endif
