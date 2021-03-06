// This file is part of the embedder test, which is intentionally built without
// NODE_WANT_INTERNALS, so we define it here manually.
#define NODE_WANT_INTERNALS 1

#include "node_native_module_env.h"

// The stub here is used when configure is run without `--code-cache-path`.
// When --code-cache-path is set this stub will not be used and instead
// an implementation will be generated. See tools/code_cache/README.md for
// more information.

namespace node {
namespace native_module {

const bool has_code_cache = false;

// The generated source code would insert <std::string, UnionString> pairs
// into NativeModuleLoader::instance.code_cache_.
void NativeModuleEnv::InitializeCodeCache() {}

}  // namespace native_module
}  // namespace node
