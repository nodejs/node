// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGLIST_H_
#define V8_REGLIST_H_

namespace v8 {
namespace internal {

// Register configurations.
#if V8_TARGET_ARCH_ARM64
typedef uint64_t RegList;
#else
typedef uint32_t RegList;
#endif

// Get the number of registers in a given register list.
inline int NumRegs(RegList list) { return base::bits::CountPopulation(list); }

}  // namespace internal
}  // namespace v8

#endif  // V8_REGLIST_H_
