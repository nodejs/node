#ifndef node_os_h
#define node_os_h

#include <node.h>
#include <v8.h>

namespace node {

class OS {
public:
  static void Initialize (v8::Handle<v8::Object> target);
};


}  // namespace node

#endif  // node_os_h
