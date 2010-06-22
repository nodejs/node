#ifndef NODE_PLATFORM_H_
#define NODE_PLATFORM_H_

namespace node {

class OS {
 public:
  static int GetMemory(size_t *rss, size_t *vsize);
  static int GetExecutablePath(char* buffer, size_t* size);
};


}  // namespace node
#endif  // NODE_PLATFORM_H_
