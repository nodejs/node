#ifdef NODE_EXPERIMENTAL_HTTP
#undef NODE_EXPERIMENTAL_HTTP
#endif

#include "node_http_parser_impl.h"
#include "node_metadata.h"

namespace node {
namespace per_process {
const char* const http_parser_version =
  NODE_STRINGIFY(HTTP_PARSER_VERSION_MAJOR)
  "."
  NODE_STRINGIFY(HTTP_PARSER_VERSION_MINOR)
  "."
  NODE_STRINGIFY(HTTP_PARSER_VERSION_PATCH);
}  // namespace per_process
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(http_parser, node::InitializeHttpParser)
