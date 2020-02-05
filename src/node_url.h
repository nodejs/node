#ifndef SRC_NODE_URL_H_
#define SRC_NODE_URL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"

#include <string>

namespace node {
namespace url {

#define PARSESTATES(XX)                                                       \
  XX(kSchemeStart)                                                            \
  XX(kScheme)                                                                 \
  XX(kNoScheme)                                                               \
  XX(kSpecialRelativeOrAuthority)                                             \
  XX(kPathOrAuthority)                                                        \
  XX(kRelative)                                                               \
  XX(kRelativeSlash)                                                          \
  XX(kSpecialAuthoritySlashes)                                                \
  XX(kSpecialAuthorityIgnoreSlashes)                                          \
  XX(kAuthority)                                                              \
  XX(kHost)                                                                   \
  XX(kHostname)                                                               \
  XX(kPort)                                                                   \
  XX(kFile)                                                                   \
  XX(kFileSlash)                                                              \
  XX(kFileHost)                                                               \
  XX(kPathStart)                                                              \
  XX(kPath)                                                                   \
  XX(kCannotBeBase)                                                           \
  XX(kQuery)                                                                  \
  XX(kFragment)

#define FLAGS(XX)                                                             \
  XX(URL_FLAGS_NONE, 0)                                                       \
  XX(URL_FLAGS_FAILED, 0x01)                                                  \
  XX(URL_FLAGS_CANNOT_BE_BASE, 0x02)                                          \
  XX(URL_FLAGS_INVALID_PARSE_STATE, 0x04)                                     \
  XX(URL_FLAGS_TERMINATED, 0x08)                                              \
  XX(URL_FLAGS_SPECIAL, 0x10)                                                 \
  XX(URL_FLAGS_HAS_USERNAME, 0x20)                                            \
  XX(URL_FLAGS_HAS_PASSWORD, 0x40)                                            \
  XX(URL_FLAGS_HAS_HOST, 0x80)                                                \
  XX(URL_FLAGS_HAS_PATH, 0x100)                                               \
  XX(URL_FLAGS_HAS_QUERY, 0x200)                                              \
  XX(URL_FLAGS_HAS_FRAGMENT, 0x400)                                           \
  XX(URL_FLAGS_IS_DEFAULT_SCHEME_PORT, 0x800)                                 \

enum url_parse_state {
  kUnknownState = -1,
#define XX(name) name,
  PARSESTATES(XX)
#undef XX
};

enum url_flags {
#define XX(name, val) name = val,
  FLAGS(XX)
#undef XX
};

struct url_data {
  int32_t flags = URL_FLAGS_NONE;
  int port = -1;
  std::string scheme;
  std::string username;
  std::string password;
  std::string host;
  std::string query;
  std::string fragment;
  std::vector<std::string> path;
};

class URL {
 public:
  static void Parse(const char* input,
                    size_t len,
                    enum url_parse_state state_override,
                    struct url_data* url,
                    bool has_url,
                    const struct url_data* base,
                    bool has_base);

  URL(const char* input, const size_t len) {
    Parse(input, len, kUnknownState, &context_, false, nullptr, false);
  }

  URL(const char* input, const size_t len, const URL* base) {
    if (base != nullptr)
      Parse(input, len, kUnknownState,
            &context_, false,
            &(base->context_), true);
    else
      Parse(input, len, kUnknownState, &context_, false, nullptr, false);
  }

  URL(const char* input, const size_t len,
      const char* base, const size_t baselen) {
    if (base != nullptr && baselen > 0) {
      URL _base(base, baselen);
      Parse(input, len, kUnknownState,
            &context_, false,
            &(_base.context_), true);
    } else {
      Parse(input, len, kUnknownState, &context_, false, nullptr, false);
    }
  }

  explicit URL(const std::string& input) :
      URL(input.c_str(), input.length()) {}

  URL(const std::string& input, const URL* base) :
      URL(input.c_str(), input.length(), base) {}

  URL(const std::string& input, const URL& base) :
      URL(input.c_str(), input.length(), &base) {}

  URL(const std::string& input, const std::string& base) :
      URL(input.c_str(), input.length(), base.c_str(), base.length()) {}

  int32_t flags() const {
    return context_.flags;
  }

  int port() const {
    return context_.port;
  }

  const std::string& protocol() const {
    return context_.scheme;
  }

  const std::string& username() const {
    return context_.username;
  }

  const std::string& password() const {
    return context_.password;
  }

  const std::string& host() const {
    return context_.host;
  }

  const std::string& query() const {
    return context_.query;
  }

  const std::string& fragment() const {
    return context_.fragment;
  }

  std::string path() const {
    std::string ret;
    for (const std::string& element : context_.path) {
      ret += '/' + element;
    }
    return ret;
  }

  // Get the path of the file: URL in a format consumable by native file system
  // APIs. Returns an empty string if something went wrong.
  std::string ToFilePath() const;
  // Get the file URL from native file system path.
  static URL FromFilePath(const std::string& file_path);

  v8::MaybeLocal<v8::Value> ToObject(Environment* env) const;

  URL(const URL&) = default;
  URL& operator=(const URL&) = default;
  URL(URL&&) = default;
  URL& operator=(URL&&) = default;

  URL() : URL("") {}

 private:
  struct url_data context_;
};

}  // namespace url

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_URL_H_
