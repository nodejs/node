#ifndef node_timer_h
#define node_timer_h

#include <ev.h>
#include <v8.h>
using namespace v8;

void node_timer_initialize (Handle<Object> target, struct ev_loop *_loop);

#endif //  node_timer_h
