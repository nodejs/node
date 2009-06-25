#ifndef node_events_h
#define node_events_h

#include "node.h"
#include <v8.h>

namespace node {

class EventEmitter : public ObjectWrap {
public:
  static void Initialize (v8::Handle<v8::Object> target);
  static v8::Persistent<v8::FunctionTemplate> constructor_template;

  bool Emit (const char *type, int argc, v8::Handle<v8::Value> argv[]);

  EventEmitter (v8::Handle<v8::Object> handle)
    : ObjectWrap(handle) { };
};

} // namespace node
#endif
