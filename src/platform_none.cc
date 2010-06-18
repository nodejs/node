#include "node.h"
#include "platform.h"


namespace node {


int OS::GetMemory(size_t *rss, size_t *vsize) {
  // Not implemented
  *rss = 0;
  *vsize = 0;
  return 0;
}


}  // namespace node
