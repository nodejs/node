#include "runtime/osx/utils.h"

namespace LIEF::runtime::osx {
std::string NSStringToUTF8(NSString* nsstring) {
  if (!nsstring) {
    return std::string();
  }
  std::string cpp_str([nsstring UTF8String]);
  return cpp_str;
}
}
