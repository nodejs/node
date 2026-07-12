#ifndef SRC_EMBEDDED_DATA_H_
#define SRC_EMBEDDED_DATA_H_

#include <cinttypes>
#include <string_view>

// This file must not depend on node.h or other code that depends on
// the full Node.js implementation because it is used during the
// compilation of the Node.js implementation itself (especially js2c).

namespace node {

std::string_view GetOctalCode(uint8_t index);

}  // namespace node

#endif  // SRC_EMBEDDED_DATA_H_
