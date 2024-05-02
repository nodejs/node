
#ifndef SRC_NODE_UNION_BYTES_H_
#define SRC_NODE_UNION_BYTES_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"

namespace node {

// An external resource intended to be used with static lifetime.
template <typename Char, typename IChar, typename Base>
class StaticExternalByteResource : public Base {
  static_assert(sizeof(IChar) == sizeof(Char),
                "incompatible interface and internal pointers");

 public:
  explicit StaticExternalByteResource(const Char* data,
                                      size_t length,
                                      std::shared_ptr<void> owning_ptr)
      : data_(data), length_(length), owning_ptr_(owning_ptr) {}

  const IChar* data() const override {
    return reinterpret_cast<const IChar*>(data_);
  }
  size_t length() const override { return length_; }

  void Dispose() override {
    // We ignore Dispose calls from V8, even if we "own" a resource via
    // owning_ptr_. All instantiations of this class are static or owned by a
    // static map, and will be destructed when static variables are destructed.
  }

  StaticExternalByteResource(const StaticExternalByteResource&) = delete;
  StaticExternalByteResource& operator=(const StaticExternalByteResource&) =
      delete;

 private:
  const Char* data_;
  const size_t length_;
  std::shared_ptr<void> owning_ptr_;
};

using StaticExternalOneByteResource =
    StaticExternalByteResource<uint8_t,
                               char,
                               v8::String::ExternalOneByteStringResource>;
using StaticExternalTwoByteResource =
    StaticExternalByteResource<uint16_t,
                               uint16_t,
                               v8::String::ExternalStringResource>;

// Similar to a v8::String, but it's independent from Isolates
// and can be materialized in Isolates as external Strings
// via ToStringChecked.
class UnionBytes {
 public:
  explicit UnionBytes(StaticExternalOneByteResource* one_byte_resource)
      : one_byte_resource_(one_byte_resource), two_byte_resource_(nullptr) {}
  explicit UnionBytes(StaticExternalTwoByteResource* two_byte_resource)
      : one_byte_resource_(nullptr), two_byte_resource_(two_byte_resource) {}

  UnionBytes(const UnionBytes&) = default;
  UnionBytes& operator=(const UnionBytes&) = default;
  UnionBytes(UnionBytes&&) = default;
  UnionBytes& operator=(UnionBytes&&) = default;

  bool is_one_byte() const { return one_byte_resource_ != nullptr; }

  v8::Local<v8::String> ToStringChecked(v8::Isolate* isolate) const;

 private:
  StaticExternalOneByteResource* one_byte_resource_;
  StaticExternalTwoByteResource* two_byte_resource_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_UNION_BYTES_H_
