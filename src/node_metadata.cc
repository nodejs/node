#include "node_metadata.h"
#include "ares.h"
#include "nghttp2/nghttp2ver.h"
#include "node.h"
#include "util.h"
#include "uv.h"
#include "v8.h"
#include "zlib.h"

#if HAVE_OPENSSL
#include "node_crypto.h"
#endif

#ifdef NODE_EXPERIMENTAL_HTTP
#include "llhttp.h"
#else /* !NODE_EXPERIMENTAL_HTTP */
#include "http_parser.h"
#endif /* NODE_EXPERIMENTAL_HTTP */

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

#ifdef NODE_EXPERIMENTAL_HTTP
  llhttp = NODE_STRINGIFY(LLHTTP_VERSION_MAJOR) "." NODE_STRINGIFY(
      LLHTTP_VERSION_MINOR) "." NODE_STRINGIFY(LLHTTP_VERSION_PATCH);
#else  /* !NODE_EXPERIMENTAL_HTTP */
  http_parser = NODE_STRINGIFY(HTTP_PARSER_VERSION_MAJOR) "." NODE_STRINGIFY(
      HTTP_PARSER_VERSION_MINOR) "." NODE_STRINGIFY(HTTP_PARSER_VERSION_PATCH);
#endif /* NODE_EXPERIMENTAL_HTTP */

#if HAVE_OPENSSL
  openssl = crypto::GetOpenSSLVersion();
#endif
}

}  // namespace node
