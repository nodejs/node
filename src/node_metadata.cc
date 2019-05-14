#include "node_metadata.h"
#include "ares.h"
#include "brotli/encode.h"
#include "nghttp2/nghttp2ver.h"
#include "node.h"
#include "util.h"
#include "uv.h"
#include "v8.h"
#include "zlib.h"

#if HAVE_OPENSSL
#include <openssl/opensslv.h>
#endif  // HAVE_OPENSSL

#ifdef NODE_HAVE_I18N_SUPPORT
#include <unicode/timezone.h>
#include <unicode/ulocdata.h>
#include <unicode/uvernum.h>
#include <unicode/uversion.h>
#endif  // NODE_HAVE_I18N_SUPPORT

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

#ifdef NODE_HAVE_I18N_SUPPORT
void Metadata::Versions::InitializeIntlVersions() {
  UErrorCode status = U_ZERO_ERROR;

  const char* tz_version = icu::TimeZone::getTZDataVersion(status);
  if (U_SUCCESS(status)) {
    tz = tz_version;
  }

  char buf[U_MAX_VERSION_STRING_LENGTH];
  UVersionInfo versionArray;
  ulocdata_getCLDRVersion(versionArray, &status);
  if (U_SUCCESS(status)) {
    u_versionToString(versionArray, buf);
    cldr = buf;
  }
}
#endif  // NODE_HAVE_I18N_SUPPORT

Metadata::Versions::Versions() {
  node = NODE_VERSION_STRING;
  v8 = v8::V8::GetVersion();
  uv = uv_version_string();
  zlib = ZLIB_VERSION;
  ares = ARES_VERSION_STR;
  modules = NODE_STRINGIFY(NODE_MODULE_VERSION);
  nghttp2 = NGHTTP2_VERSION;
  napi = NODE_STRINGIFY(NAPI_VERSION);
  llhttp = per_process::llhttp_version;
  http_parser = per_process::http_parser_version;

  brotli =
    std::to_string(BrotliEncoderVersion() >> 24) +
    "." +
    std::to_string((BrotliEncoderVersion() & 0xFFF000) >> 12) +
    "." +
    std::to_string(BrotliEncoderVersion() & 0xFFF);

#if HAVE_OPENSSL
  openssl = GetOpenSSLVersion();
#endif

#ifdef NODE_HAVE_I18N_SUPPORT
  icu = U_ICU_VERSION;
  unicode = U_UNICODE_VERSION;
#endif  // NODE_HAVE_I18N_SUPPORT
}

Metadata::Release::Release() : name(NODE_RELEASE) {
#if NODE_VERSION_IS_LTS
  lts = NODE_VERSION_LTS_CODENAME;
#endif  // NODE_VERSION_IS_LTS

#ifdef NODE_HAS_RELEASE_URLS
#define NODE_RELEASE_URLPFX NODE_RELEASE_URLBASE "v" NODE_VERSION_STRING "/"
#define NODE_RELEASE_URLFPFX NODE_RELEASE_URLPFX "node-v" NODE_VERSION_STRING

  source_url = NODE_RELEASE_URLFPFX ".tar.gz";
  headers_url = NODE_RELEASE_URLFPFX "-headers.tar.gz";
#ifdef _WIN32
  lib_url = strcmp(NODE_ARCH, "ia32") ? NODE_RELEASE_URLPFX "win-" NODE_ARCH
                                                           "/node.lib"
                                     : NODE_RELEASE_URLPFX "win-x86/node.lib";
#endif  // _WIN32

#endif  // NODE_HAS_RELEASE_URLS
}

Metadata::Metadata() : arch(NODE_ARCH), platform(NODE_PLATFORM) {}

}  // namespace node
