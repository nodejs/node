// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MATH_RANDOM_H_
#define V8_MATH_RANDOM_H_

#include "src/contexts.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class MathRandom : public AllStatic {
 public:
  static void InitializeContext(Isolate* isolate,
                                Handle<Context> native_context);

  static void ResetContext(Context* native_context);
  static Smi* RefillCache(Isolate* isolate, Context* native_context);

  static const int kCacheSize = 64;
  static const int kStateSize = 2 * kInt64Size;

  struct State {
    uint64_t s0;
    uint64_t s1;
  };
};

}  // namespace internal
}  // namespace v8
#endif  // V8_MATH_RANDOM_H_
