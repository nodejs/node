#include <stdlib.h>

#include "node.h"
#include "node_internals.h"
#include "node_url.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    node::url::URL url2(reinterpret_cast<const char*>(data), size);

    return 0;
}
