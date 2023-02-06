#ifndef SRC_NODE_URL_H_
#define SRC_NODE_URL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "ada.h"
#include "node.h"
#include "util.h"

#include <string>

namespace node {
namespace url {

std::string FromFilePath(const std::string_view file_path);

}  // namespace url
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_URL_H_
