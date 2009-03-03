#ifndef node_h
#define node_h

#include <ev.h>
#include <v8.h>

void node_fatal_exception (v8::TryCatch &try_catch); 
#define node_loop() ev_default_loop(0)

#endif // node_h

