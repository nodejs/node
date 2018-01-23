// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGLIST_H_
#define V8_REGLIST_H_

#include <cstdint>

#include "src/base/bits.h"
#include "src/base/template-utils.h"

namespace v8 {
namespace internal {

// Register configurations.
#if V8_TARGET_ARCH_ARM64
typedef uint64_t RegList;
#else
typedef uint32_t RegList;
#endif

// Get the number of registers in a given register list.
constexpr int NumRegs(RegList list) {
  return base::bits::CountPopulation(list);
}

// Combine two RegLists by building the union of the contained registers.
// Implemented as a Functor to pass it to base::fold even on gcc < 5 (see
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=52892).
// TODO(clemensh): Remove this once we require gcc >= 5.0.
struct CombineRegListsFunctor {
  constexpr RegList operator()(RegList list1, RegList list2) const {
    return list1 | list2;
  }
};

// Combine several RegLists by building the union of the contained registers.
template <typename... RegLists>
constexpr RegList CombineRegLists(RegLists... lists) {
  return base::fold(CombineRegListsFunctor{}, 0, lists...);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_REGLIST_H_
