// In V8 zlib is included as "third_party/zlib/zlib.h", which
// resolves under <(V8_ROOT). When zlib is shared, use the shim
// to forward to external zlib headers like the rest of Node.js.
#include <zlib.h>
