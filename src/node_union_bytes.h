
#ifndef SRC_NODE_UNION_BYTES_H_
#define SRC_NODE_UNION_BYTES_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

// A union of const uint8_t* or const uint16_t* data that can be
// turned into external v8::String when given an isolate.

#include "env.h"
#include "v8.h"

namespace node {

class NonOwningExternalOneByteResource
    : public v8::String::ExternalOneByteStringResource {
 public:
  explicit NonOwningExternalOneByteResource(const uint8_t* data, size_t length)
      : data_(data), length_(length) {}
  ~NonOwningExternalOneByteResource() override = default;

  const char* data() const override {
    return reinterpret_cast<const char*>(data_);
  }
  size_t length() const override { return length_; }

 private:
  const uint8_t* data_;
  size_t length_;
  DISALLOW_COPY_AND_ASSIGN(NonOwningExternalOneByteResource);
};

class NonOwningExternalTwoByteResource
    : public v8::String::ExternalStringResource {
 public:
  explicit NonOwningExternalTwoByteResource(const uint16_t* data, size_t length)
      : data_(data), length_(length) {}
  ~NonOwningExternalTwoByteResource() override = default;

  const uint16_t* data() const override { return data_; }
  size_t length() const override { return length_; }

 private:
  const uint16_t* data_;
  size_t length_;
  DISALLOW_COPY_AND_ASSIGN(NonOwningExternalTwoByteResource);
};

// Similar to a v8::String, but it's independent from Isolates
// and can be materialized in Isolates as external Strings
// via ToStringChecked. The data pointers are owned by the caller.
class UnionBytes {
 public:
  UnionBytes(const uint16_t* data, size_t length)
      : is_one_byte_(false), two_bytes_(data), length_(length) {}
  UnionBytes(const uint8_t* data, size_t length)
      : is_one_byte_(true), one_bytes_(data), length_(length) {}

  UnionBytes(const UnionBytes&) = default;
  UnionBytes& operator=(const UnionBytes&) = default;
  UnionBytes(UnionBytes&&) = default;
  UnionBytes& operator=(UnionBytes&&) = default;

  bool is_one_byte() const { return is_one_byte_; }
  const uint16_t* two_bytes_data() const {
    CHECK(!is_one_byte_);
    CHECK_NOT_NULL(two_bytes_);
    return two_bytes_;
  }
  const uint8_t* one_bytes_data() const {
    CHECK(is_one_byte_);
    CHECK_NOT_NULL(one_bytes_);
    return one_bytes_;
  }
  v8::Local<v8::String> ToStringChecked(v8::Isolate* isolate) const {
    if (is_one_byte_) {
      CHECK_NOT_NULL(one_bytes_);
      NonOwningExternalOneByteResource* source =
          new NonOwningExternalOneByteResource(one_bytes_, length_);
      return v8::String::NewExternalOneByte(isolate, source).ToLocalChecked();
    } else {
      CHECK_NOT_NULL(two_bytes_);
      NonOwningExternalTwoByteResource* source =
          new NonOwningExternalTwoByteResource(two_bytes_, length_);
      return v8::String::NewExternalTwoByte(isolate, source).ToLocalChecked();
    }
  }
  size_t length() { return length_; }

 private:
  bool is_one_byte_;
  union {
    const uint8_t* one_bytes_;
    const uint16_t* two_bytes_;
  };
  size_t length_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_UNION_BYTES_H_
