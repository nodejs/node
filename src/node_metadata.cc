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
#include "node_crypto.h"
#endif

namespace node {

namespace per_process {
Metadata metadata;
}

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
  openssl = crypto::GetOpenSSLVersion();
#endif
}

}  // namespace node
