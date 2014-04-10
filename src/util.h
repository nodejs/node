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

#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include "v8.h"
#include "string_bytes.h"

namespace node {

class Utf8Value {
  public:
    explicit Utf8Value(v8::Handle<v8::Value> value)
      : length_(0), str_(NULL) {
      if (value.IsEmpty())
        return;

      v8::Local<v8::String> val_ = value->ToString();

      // Allocate enough space to include the null terminator
      size_t len = StringBytes::StorageSize(val_, UTF8) + 1;

      char* str = static_cast<char*>(calloc(1, len));

      int flags = WRITE_UTF8_FLAGS;
      flags |= ~v8::String::NO_NULL_TERMINATION;

      length_ = val_->WriteUtf8(str,
                                len,
                                0,
                                flags);

      str_ = reinterpret_cast<char*>(str);
    }

    ~Utf8Value() {
      free(str_);
    }

    char* operator*() {
      return str_;
    };

    const char* operator*() const {
      return str_;
    };

    size_t length() {
      return length_;
    };

  private:
    size_t length_;
    char* str_;
};

}  // namespace node

#endif  // SRC_UTIL_H_
