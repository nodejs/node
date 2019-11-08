// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_REGLIST_H_
#define V8_CODEGEN_REGLIST_H_

#include <cstdint>

#include "src/base/bits.h"
#include "src/base/template-utils.h"

namespace v8 {
namespace internal {

// Register configurations.
#if V8_TARGET_ARCH_ARM64
using RegList = uint64_t;
#else
using RegList = uint32_t;
#endif

// Get the number of registers in a given register list.
constexpr int NumRegs(RegList list) {
  return base::bits::CountPopulation(list);
}

namespace detail {
// Combine two RegLists by building the union of the contained registers.
// TODO(clemensb): Replace by constexpr lambda once we have C++17.
constexpr RegList CombineRegListsHelper(RegList list1, RegList list2) {
  return list1 | list2;
}
}  // namespace detail

// Combine several RegLists by building the union of the contained registers.
template <typename... RegLists>
constexpr RegList CombineRegLists(RegLists... lists) {
  return base::fold(detail::CombineRegListsHelper, 0, lists...);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_REGLIST_H_
