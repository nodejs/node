#ifndef node_file_h
#define node_file_h

#include <v8.h>

namespace node {

class File {
public:
  static void Initialize (v8::Handle<v8::Object> target);
};

} // namespace node
#endif // node_file_h
