// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_TICK_COUNTER_H_
#define V8_CODEGEN_TICK_COUNTER_H_

#include <cstddef>

namespace v8 {
namespace internal {

// A deterministic correlate of time, used to detect performance or
// divergence bugs in Turbofan. DoTick() should be called frequently
// thoughout the compilation.
class TickCounter {
 public:
  void DoTick();
  size_t CurrentTicks() const { return ticks_; }

 private:
  size_t ticks_ = 0;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_TICK_COUNTER_H_
