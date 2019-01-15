#include <node.h>

NODE_MODULE_INIT(/*exports, module, context*/) {}

NODE_MODULE_DECLARE_ABI({node_abi_icu_version, NODE_ABI_ICU_VERSION + 1})
