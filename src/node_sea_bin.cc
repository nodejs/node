#include "node_sea.h"

#ifdef HAVE_LIEF
#include "LIEF/LIEF.hpp"
#endif  // HAVE_LIEF

#include "debug_utils-inl.h"
#include "env-inl.h"
#include "util-inl.h"

#include <algorithm>
#include <codecvt>
#include <locale>
#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string_view>

// The POSTJECT_SENTINEL_FUSE macro is a string of random characters selected by
// the Node.js project that is present only once in the entire binary. It is
// used by the postject_has_resource() function to efficiently detect if a
// resource has been injected. See
// https://github.com/nodejs/postject/blob/35343439cac8c488f2596d7c4c1dddfec1fddcae/postject-api.h#L42-L45.
#define POSTJECT_SENTINEL_FUSE "NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2"
#include "postject-api.h"
#undef POSTJECT_SENTINEL_FUSE

namespace node {
namespace sea {

std::string_view FindSingleExecutableBlob() {
#if !defined(DISABLE_SINGLE_EXECUTABLE_APPLICATION)
  CHECK(IsSingleExecutable());
  static const std::string_view result = []() -> std::string_view {
    size_t size;
#ifdef __APPLE__
    postject_options options;
    postject_options_init(&options);
    options.macho_segment_name = "NODE_SEA";
    const char* blob = static_cast<const char*>(
        postject_find_resource("NODE_SEA_BLOB", &size, &options));
#else
    const char* blob = static_cast<const char*>(
        postject_find_resource("NODE_SEA_BLOB", &size, nullptr));
#endif
    return {blob, size};
  }();
  per_process::Debug(DebugCategory::SEA,
                     "Found SEA blob %p, size=%zu\n",
                     result.data(),
                     result.size());
  return result;
#else
  UNREACHABLE();
#endif  // !defined(DISABLE_SINGLE_EXECUTABLE_APPLICATION)
}

bool IsSingleExecutable() {
  return postject_has_resource();
}

}  // namespace sea
}  // namespace node
