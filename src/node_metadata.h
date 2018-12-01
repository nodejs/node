#ifndef SRC_NODE_METADATA_H_
#define SRC_NODE_METADATA_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <string>

namespace node {

#define NODE_VERSIONS_KEYS_BASE(V)                                             \
  V(node)                                                                      \
  V(v8)                                                                        \
  V(uv)                                                                        \
  V(zlib)                                                                      \
  V(ares)                                                                      \
  V(modules)                                                                   \
  V(nghttp2)                                                                   \
  V(napi)

#ifdef NODE_EXPERIMENTAL_HTTP
#define NODE_VERSIONS_KEY_HTTP(V) V(llhttp)
#else /* !NODE_EXPERIMENTAL_HTTP */
#define NODE_VERSIONS_KEY_HTTP(V) V(http_parser)
#endif /* NODE_EXPERIMENTAL_HTTP */

#if HAVE_OPENSSL
#define NODE_VERSIONS_KEY_CRYPTO(V) V(openssl)
#else
#define NODE_VERSIONS_KEY_CRYPTO(V)
#endif

#define NODE_VERSIONS_KEYS(V)                                                  \
  NODE_VERSIONS_KEYS_BASE(V)                                                   \
  NODE_VERSIONS_KEY_HTTP(V)                                                    \
  NODE_VERSIONS_KEY_CRYPTO(V)

class Metadata {
 public:
  struct Versions {
    Versions();
#define V(key) std::string key;
    NODE_VERSIONS_KEYS(V)
#undef V
  };

  Versions versions;
};

// Per-process global
namespace per_process {
extern Metadata metadata;
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_NODE_METADATA_H_
