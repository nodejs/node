// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_SUPERSPREAD_H_
#define V8_BUILTINS_SUPERSPREAD_H_

#include "src/objects/objects.h"

namespace v8 {
namespace internal {

struct SuperSpreadArgs {
#ifdef V8_TARGET_ARCH_ARM64
  static constexpr int kNumExtraArgs = 5;
#else
  static constexpr int kNumExtraArgs = 4;
#endif

  static constexpr int kReceiverOffsetFromEnd = kNumExtraArgs;
  static constexpr int kTargetOffsetFromEnd = kNumExtraArgs - 1;
  static constexpr int kArglistOffsetFromEnd = kNumExtraArgs - 2;

  static constexpr int kArglistLengthOffsetFromEnd = 1;
};

// TODO(olivf): Support more builtins.
#define SUPERSPREAD_BUILTINS(V) V(ArrayPrototypePush, GenericArrayPushVararg)

#define DECLARE_SUPER_SPREAD_HANDLER(_, Name)             \
  V8_EXPORT_PRIVATE Tagged<Object> Name(Isolate* isolate, \
                                        RuntimeArguments& args);
SUPERSPREAD_BUILTINS(DECLARE_SUPER_SPREAD_HANDLER)
#undef DECLARE_SUPER_SPREAD_HANDLER

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_SUPERSPREAD_H_
