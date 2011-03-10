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

#include <node_events.h>
#include <node.h>

namespace node {

using namespace v8;

Persistent<FunctionTemplate> EventEmitter::constructor_template;

static Persistent<String> events_symbol;

void EventEmitter::Initialize(Local<FunctionTemplate> ctemplate) {
  HandleScope scope;

  constructor_template = Persistent<FunctionTemplate>::New(ctemplate);

  constructor_template->SetClassName(String::NewSymbol("EventEmitter"));

  events_symbol = NODE_PSYMBOL("_events");

  // All other prototype methods are defined in events.js
}


bool EventEmitter::Emit(Handle<String> event, int argc, Handle<Value> argv[]) {
  HandleScope scope;
  // HandleScope not needed here because only called from one of the two
  // functions below
  Local<Value> events_v = handle_->Get(events_symbol);
  if (!events_v->IsObject()) return false;
  Local<Object> events = events_v->ToObject();

  Local<Value> listeners_v = events->Get(event);

  TryCatch try_catch;

  if (listeners_v->IsFunction()) {
    // Optimized one-listener case
    Local<Function> listener = Local<Function>::Cast(listeners_v);

    listener->Call(handle_, argc, argv);

    if (try_catch.HasCaught()) {
      FatalException(try_catch);
      return false;
    }

  } else if (listeners_v->IsArray()) {
    Local<Array> listeners = Local<Array>::Cast(listeners_v->ToObject()->Clone());

    for (uint32_t i = 0; i < listeners->Length(); i++) {
      Local<Value> listener_v = listeners->Get(i);
      if (!listener_v->IsFunction()) continue;
      Local<Function> listener = Local<Function>::Cast(listener_v);

      listener->Call(handle_, argc, argv);

      if (try_catch.HasCaught()) {
        FatalException(try_catch);
        return false;
      }
    }

  } else {
    return false;
  }

  return true;
}

}  // namespace node
