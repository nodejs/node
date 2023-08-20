// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_COMPRESSED_ZONE_PTR_H_
#define V8_ZONE_COMPRESSED_ZONE_PTR_H_

#include <type_traits>

#include "src/base/logging.h"
#include "src/common/globals.h"
#include "src/zone/zone-compression.h"

namespace v8 {
namespace internal {

//
// Compressed pointer to T using aligned-base-relative addressing compression.
//
// Note that the CompressedZonePtr<T> is implicitly convertible to T*.
// Such an approach provides the benefit of almost seamless migration of a code
// using full pointers to compressed pointers.
// However, using CompressedZonePtr<T> in containers is not allowed yet.
//
// It's not recommended to use this class directly, use ZoneTypeTraits::Ptr<T>
// instead.
template <typename T>
class CompressedZonePtr {
 public:
  CompressedZonePtr() = default;
  explicit CompressedZonePtr(std::nullptr_t) : CompressedZonePtr() {}
  explicit CompressedZonePtr(T* value) { *this = value; }
  // Move- and copy-constructors are explicitly deleted in order to avoid
  // creation of temporary objects which we can't uncompress because they will
  // live outside of the zone memory.
  CompressedZonePtr(const CompressedZonePtr& other) V8_NOEXCEPT = delete;
  CompressedZonePtr(CompressedZonePtr&&) V8_NOEXCEPT = delete;

  CompressedZonePtr& operator=(const CompressedZonePtr& other) V8_NOEXCEPT {
    DCHECK(ZoneCompression::CheckSameBase(this, &other));
    compressed_value_ = other.compressed_value_;
    return *this;
  }
  CompressedZonePtr& operator=(CompressedZonePtr&& other) V8_NOEXCEPT = delete;

  CompressedZonePtr& operator=(T* value) {
    compressed_value_ = ZoneCompression::Compress(value);
    DCHECK_EQ(value, Decompress());
    return *this;
  }

  bool operator==(std::nullptr_t) const { return compressed_value_ == 0; }
  bool operator!=(std::nullptr_t) const { return compressed_value_ != 0; }

  // The equality comparisons assume that both operands point to objects
  // allocated by the same allocator supporting pointer compression, therefore
  // it's enough to compare compressed values.
  bool operator==(const CompressedZonePtr& other) const {
    return compressed_value_ == other.compressed_value_;
  }
  bool operator!=(const CompressedZonePtr& other) const {
    return !(*this == other);
  }
  bool operator==(T* other) const {
    return compressed_value_ == ZoneCompression::Compress(other);
  }
  bool operator!=(T* other) const { return !(*this == other); }

  T& operator*() const { return *Decompress(); }
  T* operator->() const { return Decompress(); }

  operator T*() const { return Decompress(); }
  operator bool() const { return compressed_value_ != 0; }

 private:
  T* Decompress() const {
    return reinterpret_cast<T*>(
        ZoneCompression::Decompress(this, compressed_value_));
  }

  uint32_t compressed_value_ = 0;
};

// This requirement is necessary for being able to use memcopy in containers
// of zone pointers.
// TODO(ishell): Re-enable once compressed pointers are supported in containers.
// static_assert(std::is_trivially_copyable<CompressedZonePtr<int>>::value,
//               "CompressedZonePtr must be trivially copyable");

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_COMPRESSED_ZONE_PTR_H_
