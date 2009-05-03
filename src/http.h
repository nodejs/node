#ifndef node_http_h
#define node_http_h

#include <v8.h>
#include "net.h"

namespace node {

class HTTPClient : public node::Connection {
public:
  static void Initialize (v8::Handle<v8::Object> target);

  HTTPClient (v8::Handle<v8::Object> handle, v8::Handle<v8::Object> protocol);

  static v8::Handle<v8::Value> v8New (const v8::Arguments& args);
protected:
  void OnReceive (const void *buf, size_t len);

};

} // namespace node
#endif
