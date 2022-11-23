#include "node_metadata.h"
#include "ares.h"
#include "brotli/encode.h"
#include "llhttp.h"
#include "nghttp2/nghttp2ver.h"
#include "node.h"
#include "util.h"
#include "uv.h"
#include "v8.h"
#include "zlib.h"
#include "fstream"
#include "sstream"
#include "filesystem"

#if HAVE_OPENSSL
#include <openssl/opensslv.h>
#if NODE_OPENSSL_HAS_QUIC
#include <openssl/quic.h>
#endif
#endif  // HAVE_OPENSSL

#ifdef OPENSSL_INFO_QUIC
#include <ngtcp2/version.h>
#include <nghttp3/version.h>
#endif

#ifdef NODE_HAVE_I18N_SUPPORT
#include <unicode/timezone.h>
#include <unicode/ulocdata.h>
#include <unicode/uvernum.h>
#include <unicode/uversion.h>
#endif  // NODE_HAVE_I18N_SUPPORT
#include <iostream>

namespace node {

namespace per_process {
Metadata metadata;
}

#if HAVE_OPENSSL
static constexpr size_t search(const char* s, char c, size_t n = 0) {
  return *s == c ? n : search(s + 1, c, n + 1);
}

static inline std::string GetOpenSSLVersion() {
  // sample openssl version string format
  // for reference: "OpenSSL 1.1.0i 14 Aug 2018"
  constexpr size_t start = search(OPENSSL_VERSION_TEXT, ' ') + 1;
  constexpr size_t len = search(&OPENSSL_VERSION_TEXT[start], ' ');
  return std::string(OPENSSL_VERSION_TEXT, start, len);
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
  undici = Metadata::Versions::GetDependencyVersionFromPackageJson("undici");
  node = NODE_VERSION_STRING;
  v8 = v8::V8::GetVersion();
  uv = uv_version_string();
  zlib = ZLIB_VERSION;
  ares = ARES_VERSION_STR;
  modules = NODE_STRINGIFY(NODE_MODULE_VERSION);
  nghttp2 = NGHTTP2_VERSION;
  napi = NODE_STRINGIFY(NAPI_VERSION);
  llhttp =
      NODE_STRINGIFY(LLHTTP_VERSION_MAJOR)
      "."
      NODE_STRINGIFY(LLHTTP_VERSION_MINOR)
      "."
      NODE_STRINGIFY(LLHTTP_VERSION_PATCH);

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

#ifdef OPENSSL_INFO_QUIC
  ngtcp2 = NGTCP2_VERSION;
  nghttp3 = NGHTTP3_VERSION;
#endif
}

bool hasEnding(std::string const& fullString, std::string const& ending) {
  if (fullString.length() >= ending.length()) {
    return (0 == fullString.compare(fullString.length() - ending.length(),
                                    ending.length(),
                                    ending));
  } else {
    return false;
  }
}

std::string Metadata::Versions::GetDependencyVersionFromPackageJson(
    std::string packageName) {

  // get current path and find parent which has ending 'node'
  std::filesystem::path cwd = std::filesystem::current_path();
  while (!hasEnding(cwd.parent_path().string(), "node")) {
    cwd = cwd.parent_path();
  }
  std::ifstream myFile(cwd.parent_path().string()+"\\deps\\" + packageName + "\\src\\package.json");
  std::ostringstream tmp;
  tmp << myFile.rdbuf();
  std::string fileString = tmp.str();

  // 9 is character count of \"version\" and plus 1"
  int ix10 = fileString.find("\"version\"") + 9;
  std::string version = "";
  bool startAdding = false;
  for (int i = ix10; i < fileString.length(); i++) {

    if (fileString[i] == '\"' && !startAdding) {
      startAdding = true;
    }else if (fileString[i] == '\"' && startAdding) {
      break;
    }
    if (fileString[i] != '\"' && startAdding) {
      version = version + fileString[i];
    }
  }

  return version;
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
