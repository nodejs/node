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
  // Function object that compares the start address of two regions. Usable as
  // compare function on std data structures and algorithms.
  struct StartAddressLess {
    bool operator()(base::AddressRegion a, base::AddressRegion b) const {
      return a.begin() < b.begin();
    }
  };

  using Address = uintptr_t;

  constexpr AddressRegion() = default;

  constexpr AddressRegion(Address address, size_t size)
      : address_(address), size_(size) {}

  Address begin() const { return address_; }
  Address end() const { return address_ + size_; }

  size_t size() const { return size_; }
  void set_size(size_t size) { size_ = size; }

  bool is_empty() const { return size_ == 0; }

  bool contains(Address address) const {
    static_assert(std::is_unsigned<Address>::value);
    return (address - begin()) < size();
  }

  bool contains(Address address, size_t size) const {
    static_assert(std::is_unsigned<Address>::value);
    Address offset = address - begin();
    return (offset < size_) && (offset + size <= size_);
  }

  bool contains(AddressRegion region) const {
    return contains(region.address_, region.size_);
  }

  base::AddressRegion GetOverlap(AddressRegion region) const {
    Address overlap_start = std::max(begin(), region.begin());
    Address overlap_end =
        std::max(overlap_start, std::min(end(), region.end()));
    return {overlap_start, overlap_end - overlap_start};
  }

  bool operator==(AddressRegion other) const {
    return address_ == other.address_ && size_ == other.size_;
  }

 private:
  Address address_ = 0;
  size_t size_ = 0;
};
ASSERT_TRIVIALLY_COPYABLE(AddressRegion);

// Construct an AddressRegion from a start pointer and a size.
template <typename T>
inline AddressRegion AddressRegionOf(T* ptr, size_t size) {
  return AddressRegion{reinterpret_cast<AddressRegion::Address>(ptr),
                       sizeof(T) * size};
}

// Construct an AddressRegion from anything providing a {data()} and {size()}
// accessor.
template <typename Container>
inline auto AddressRegionOf(Container&& c)
    -> decltype(AddressRegionOf(c.data(), c.size())) {
  return AddressRegionOf(c.data(), c.size());
}

inline std::ostream& operator<<(std::ostream& out, AddressRegion region) {
  return out << "[" << reinterpret_cast<void*>(region.begin()) << "+"
             << region.size() << "]";
}

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_ADDRESS_REGION_H_
