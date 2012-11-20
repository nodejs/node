// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "node_counters.h"

#include "uv.h"

#include <string.h>


namespace node {

using namespace v8;


static uint64_t counter_gc_start_time;
static uint64_t counter_gc_end_time;

#define SLURP_OBJECT(obj, member, valp) \
  if (!(obj)->IsObject()) { \
    return (ThrowException(Exception::Error(String::New("expected " \
      "object for " #obj " to contain object member " #member)))); \
  } \
  *valp = Local<Object>::Cast(obj->Get(String::New(#member)));


Handle<Value> COUNTER_NET_SERVER_CONNECTION(const Arguments& args) {
  NODE_COUNT_SERVER_CONN_OPEN();
  return Undefined();
}


Handle<Value> COUNTER_NET_SERVER_CONNECTION_CLOSE(const Arguments& args) {
  NODE_COUNT_SERVER_CONN_CLOSE();
  return Undefined();
}


Handle<Value> COUNTER_HTTP_SERVER_REQUEST(const Arguments& args) {
  NODE_COUNT_HTTP_SERVER_REQUEST();
  return Undefined();
}


Handle<Value> COUNTER_HTTP_SERVER_RESPONSE(const Arguments& args) {
  NODE_COUNT_HTTP_SERVER_RESPONSE();
  return Undefined();
}


Handle<Value> COUNTER_HTTP_CLIENT_REQUEST(const Arguments& args) {
  NODE_COUNT_HTTP_CLIENT_REQUEST();
  return Undefined();
}


Handle<Value> COUNTER_HTTP_CLIENT_RESPONSE(const Arguments& args) {
  NODE_COUNT_HTTP_CLIENT_RESPONSE();
  return Undefined();
}


static void counter_gc_start(GCType type, GCCallbackFlags flags) {
  counter_gc_start_time = NODE_COUNT_GET_GC_RAWTIME();

  return;
}


static void counter_gc_done(GCType type, GCCallbackFlags flags) {
  uint64_t endgc = NODE_COUNT_GET_GC_RAWTIME();
  if (endgc != 0) {
    uint64_t totalperiod = endgc - counter_gc_end_time;
    uint64_t gcperiod = endgc - counter_gc_start_time;

    if (totalperiod > 0) {
      unsigned int percent = static_cast<unsigned int>((gcperiod * 100) / totalperiod);

      NODE_COUNT_GC_PERCENTTIME(percent);
      counter_gc_end_time = endgc;
    }
  }

  return;
}


#define NODE_PROBE(name) #name, name

void InitPerfCounters(Handle<Object> target) {
  HandleScope scope;

  static struct {
    const char* name;
    Handle<Value> (*func)(const Arguments&);
    Persistent<FunctionTemplate> templ;
  } tab[] = {
    { NODE_PROBE(COUNTER_NET_SERVER_CONNECTION) },
    { NODE_PROBE(COUNTER_NET_SERVER_CONNECTION_CLOSE) },
    { NODE_PROBE(COUNTER_HTTP_SERVER_REQUEST) },
    { NODE_PROBE(COUNTER_HTTP_SERVER_RESPONSE) },
    { NODE_PROBE(COUNTER_HTTP_CLIENT_REQUEST) },
    { NODE_PROBE(COUNTER_HTTP_CLIENT_RESPONSE) }
  };

  for (int i = 0; i < ARRAY_SIZE(tab); i++) {
    tab[i].templ = Persistent<FunctionTemplate>::New(
        FunctionTemplate::New(tab[i].func));
    target->Set(String::NewSymbol(tab[i].name), tab[i].templ->GetFunction());
  }

  // Only Windows performance counters supported
  // To enable other OS, use conditional compilation here
  InitPerfCountersWin32();

  // init times for GC percent calculation and hook callbacks
  counter_gc_start_time = NODE_COUNT_GET_GC_RAWTIME();
  counter_gc_end_time = counter_gc_start_time;

  v8::V8::AddGCPrologueCallback(counter_gc_start);
  v8::V8::AddGCEpilogueCallback(counter_gc_done);
}


void TermPerfCounters(Handle<Object> target) {
  // Only Windows performance counters supported
  // To enable other OS, use conditional compilation here
  TermPerfCountersWin32();
}

}
