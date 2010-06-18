#ifndef NODE_PLATFORM_H_
#define NODE_PLATFORM_H_

namespace node {

class OS {
 public:
  static int GetMemory(size_t *rss, size_t *vsize);
};


}  // namespace node
#endif  // NODE_PLATFORM_H_
