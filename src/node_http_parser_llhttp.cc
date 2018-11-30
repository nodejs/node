#define NODE_EXPERIMENTAL_HTTP 1

#include "node_http_parser_impl.h"

namespace node {

const char* llhttp_version =
    NODE_STRINGIFY(LLHTTP_VERSION_MAJOR)
    "."
    NODE_STRINGIFY(LLHTTP_VERSION_MINOR)
    "."
    NODE_STRINGIFY(LLHTTP_VERSION_PATCH);

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(http_parser_llhttp,
                                   node::InitializeHttpParser)
