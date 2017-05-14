
#ifndef SRC_STRING_UTILS_H_
#define SRC_STRING_UTILS_H_

#include "env.h"
#include "env-inl.h"
#include "util.h"

namespace node {
namespace stringutils {
  bool ContainsNonAscii(const char* src, size_t len);
}  // namespace stringutils
}  // namespace node

#endif  // SRC_STRING_UTILS_H_
