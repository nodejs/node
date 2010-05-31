#ifndef node_stdio_h
#define node_stdio_h

#include <node.h>
#include <v8.h>

namespace node {

class Stdio  {
public:
  static void Initialize (v8::Handle<v8::Object> target);
  static void Flush ();
  static void DisableRawMode(int fd);
};

}  // namespace node
#endif  // node_stdio_h
