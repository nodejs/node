// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_ADDRESS_REGION_H_
#define V8_BASE_ADDRESS_REGION_H_

#include <iostream>

#include "src/base/macros.h"

namespace v8 {
namespace base {

// Helper class representing an address region of certain size.
class AddressRegion {
 public:
  using Address = uintptr_t;

  AddressRegion() = default;

  AddressRegion(Address address, size_t size)
      : address_(address), size_(size) {}

  Address begin() const { return address_; }
  Address end() const { return address_ + size_; }

  size_t size() const { return size_; }
  void set_size(size_t size) { size_ = size; }

  bool is_empty() const { return size_ == 0; }

  bool contains(Address address) const {
    STATIC_ASSERT(std::is_unsigned<Address>::value);
    return (address - begin()) < size();
  }

  bool contains(Address address, size_t size) const {
    STATIC_ASSERT(std::is_unsigned<Address>::value);
    Address offset = address - begin();
    return (offset < size_) && (offset + size <= size_);
  }

  bool contains(AddressRegion region) const {
    return contains(region.address_, region.size_);
  }

  bool operator==(AddressRegion other) const {
    return address_ == other.address_ && size_ == other.size_;
  }

  bool operator!=(AddressRegion other) const {
    return address_ != other.address_ || size_ != other.size_;
  }

 private:
  Address address_ = 0;
  size_t size_ = 0;
};
ASSERT_TRIVIALLY_COPYABLE(AddressRegion);

inline std::ostream& operator<<(std::ostream& out, AddressRegion region) {
  return out << "[" << reinterpret_cast<void*>(region.begin()) << "+"
             << region.size() << "]";
}

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_ADDRESS_REGION_H_
