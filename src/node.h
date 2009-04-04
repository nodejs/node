#ifndef node_h
#define node_h

#include <ev.h>
#include <eio.h>
#include <v8.h>

void node_fatal_exception (v8::TryCatch &try_catch); 
#define node_loop() ev_default_loop(0)

// call this after creating a new eio event.
void node_eio_submit(eio_req *req);

#endif // node_h

