// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

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
