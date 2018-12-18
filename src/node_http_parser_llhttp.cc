#define NODE_EXPERIMENTAL_HTTP 1

#include "node_http_parser_impl.h"
#include "node_metadata.h"

namespace node {

namespace per_process {
const char* const llhttp_version =
    NODE_STRINGIFY(LLHTTP_VERSION_MAJOR)
    "."
    NODE_STRINGIFY(LLHTTP_VERSION_MINOR)
    "."
    NODE_STRINGIFY(LLHTTP_VERSION_PATCH);
}  // namespace per_process
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(http_parser_llhttp,
                                   node::InitializeHttpParser)
