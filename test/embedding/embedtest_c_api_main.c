#include "node_embed_api.h"

// The simplest Node.js embedding scenario where the Node.js start function is
// invoked from the libnode shared library as it would be run from the Node.js
// CLI.
int32_t test_main_c_api_nodejs_main(int32_t argc, char* argv[]) {
  return node_embed_start(argc, argv);
}
