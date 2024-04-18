// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRACING_PERFETTO_UTILS_H_
#define V8_TRACING_PERFETTO_UTILS_H_

#include <cstdint>
#include <vector>

#include "include/v8config.h"
#include "src/base/logging.h"
#include "src/objects/string.h"
#include "src/objects/tagged.h"

namespace v8 {
namespace internal {

// Helper class to write String objects into Perfetto protos. Deals with
// character encoding and String objects composed of multiple slices.
class PerfettoV8String {
 public:
  explicit PerfettoV8String(Tagged<String> string);

  bool is_one_byte() const { return visitor_.is_one_byte_; }
  const std::string& buffer() const { return visitor_.buffer_; }
  template <typename Proto>
  void WriteToProto(Proto& proto) {
    if (is_one_byte()) {
      proto.set_latin1(buffer());
    } else {
#if defined(V8_TARGET_BIG_ENDIAN)
      proto.set_utf16_be(buffer());
#else
      proto.set_utf16_le(buffer());
#endif
    }
  }

 private:
  struct Visitor {
    void VisitOneByteString(const uint8_t* chars, int length) {
      CHECK(!is_two_byte_);
      is_one_byte_ = true;
      const char* first = reinterpret_cast<const char*>(chars);
      const char* last = reinterpret_cast<const char*>(chars + length);
      buffer_.append(first, last);
    }

    void VisitTwoByteString(const uint16_t* chars, int length) {
      CHECK(!is_one_byte_);
      is_two_byte_ = true;
      const char* first = reinterpret_cast<const char*>(chars);
      const char* last = reinterpret_cast<const char*>(chars + length);
      buffer_.append(first, last);
    }

    // Note: This should really be a std::vector<uint8_t> but this needs to be
    // hashed and having a std::string is just convenient as we can use the
    // standard std::hash<string>
    std::string buffer_;
    bool is_one_byte_{false};
    bool is_two_byte_{false};
  };
  Visitor visitor_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_TRACING_PERFETTO_UTILS_H_
