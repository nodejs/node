#ifndef SRC_NODE_STRING_H_
#define SRC_NODE_STRING_H_

#include <v8.h>

namespace node {

#define IMMUTABLE_STRING(string_literal)                                \
  ::node::ImmutableAsciiSource::CreateFromLiteral(                      \
      string_literal "", sizeof(string_literal) - 1)
#define BUILTIN_ASCII_ARRAY(array, len)                                 \
  ::node::ImmutableAsciiSource::CreateFromLiteral(array, len)

class ImmutableAsciiSource : public v8::String::ExternalAsciiStringResource {
 public:
  static v8::Handle<v8::String> CreateFromLiteral(const char *string_literal,
                                                  size_t length);

  ImmutableAsciiSource(const char *src, size_t src_len)
      : buffer_(src),
        buf_len_(src_len) {
  }

  ~ImmutableAsciiSource() {
  }

  const char *data() const {
      return buffer_;
  }

  size_t length() const {
      return buf_len_;
  }

 private:
  const char *buffer_;
  size_t buf_len_;
};

}  // namespace node

#endif  // SRC_NODE_STRING_H_
