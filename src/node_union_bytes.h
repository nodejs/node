
#ifndef SRC_NODE_UNION_BYTES_H_
#define SRC_NODE_UNION_BYTES_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

// A union of const uint8_t* or const uint16_t* data that can be
// turned into external v8::String when given an isolate.

#include "v8.h"

namespace node {

// Similar to a v8::String, but it's independent from Isolates
// and can be materialized in Isolates as external Strings
// via ToStringChecked.
class UnionBytes {
 public:
  UnionBytes(const uint16_t* data, size_t length)
      : one_bytes_(nullptr), two_bytes_(data), length_(length) {}
  UnionBytes(const uint8_t* data, size_t length)
      : one_bytes_(data), two_bytes_(nullptr), length_(length) {}
  template <typename T>  // T = uint8_t or uint16_t
  explicit UnionBytes(std::shared_ptr<std::vector</*const*/ T>> data)
      : UnionBytes(data->data(), data->size()) {
    owning_ptr_ = data;
  }

  UnionBytes(const UnionBytes&) = default;
  UnionBytes& operator=(const UnionBytes&) = default;
  UnionBytes(UnionBytes&&) = default;
  UnionBytes& operator=(UnionBytes&&) = default;

  bool is_one_byte() const { return one_bytes_ != nullptr; }
  const uint16_t* two_bytes_data() const {
    CHECK_NOT_NULL(two_bytes_);
    return two_bytes_;
  }
  const uint8_t* one_bytes_data() const {
    CHECK_NOT_NULL(one_bytes_);
    return one_bytes_;
  }
  v8::Local<v8::String> ToStringChecked(v8::Isolate* isolate) const;
  size_t length() const { return length_; }

 private:
  const uint8_t* one_bytes_;
  const uint16_t* two_bytes_;
  size_t length_;
  std::shared_ptr<void> owning_ptr_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_UNION_BYTES_H_
