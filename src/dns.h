#ifndef node_dns_h
#define node_dns_h

#include <v8.h>

namespace node {

class DNS {
 public:
  static void Initialize(v8::Handle<v8::Object> target);
};

} // namespace node
#endif // node_dns_h
