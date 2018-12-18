#include "node_metadata.h"
#include "ares.h"
#include "nghttp2/nghttp2ver.h"
#include "node.h"
#include "node_internals.h"
#include "util.h"
#include "uv.h"
#include "v8.h"
#include "zlib.h"

#if HAVE_OPENSSL
#include <openssl/opensslv.h>
#endif  // HAVE_OPENSSL

namespace node {

namespace per_process {
Metadata metadata;
}

#if HAVE_OPENSSL
constexpr int search(const char* s, int n, int c) {
  return *s == c ? n : search(s + 1, n + 1, c);
}

std::string GetOpenSSLVersion() {
  // sample openssl version string format
  // for reference: "OpenSSL 1.1.0i 14 Aug 2018"
  char buf[128];
  const int start = search(OPENSSL_VERSION_TEXT, 0, ' ') + 1;
  const int end = search(OPENSSL_VERSION_TEXT + start, start, ' ');
  const int len = end - start;
  snprintf(buf, sizeof(buf), "%.*s", len, &OPENSSL_VERSION_TEXT[start]);
  return std::string(buf);
}
#endif  // HAVE_OPENSSL

Metadata::Versions::Versions() {
  node = NODE_VERSION_STRING;
  v8 = v8::V8::GetVersion();
  uv = uv_version_string();
  zlib = ZLIB_VERSION;
  ares = ARES_VERSION_STR;
  modules = NODE_STRINGIFY(NODE_MODULE_VERSION);
  nghttp2 = NGHTTP2_VERSION;
  napi = NODE_STRINGIFY(NAPI_VERSION);
  llhttp = llhttp_version;
  http_parser = http_parser_version;

#if HAVE_OPENSSL
  openssl = GetOpenSSLVersion();
#endif
}

}  // namespace node
