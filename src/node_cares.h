#ifndef NODE_CARES_H_
#define NODE_CARES_H_

#include <node.h>
#include <v8.h>
#include <ares.h>

namespace node {

class Cares {
 public:
  static void Initialize(v8::Handle<v8::Object> target);
};

}  // namespace node
#endif  // NODE_CARES_H_
