// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arguments.h"

namespace v8 {
namespace internal {

double ClobberDoubleRegisters(double x1, double x2, double x3, double x4) {
  // TODO(ulan): This clobbers only subset of registers depending on compiler,
  // Rewrite this in assembly to really clobber all registers.
  // GCC for ia32 uses the FPU and does not touch XMM registers.
  return x1 * 1.01 + x2 * 2.02 + x3 * 3.03 + x4 * 4.04;
}

}  // namespace internal
}  // namespace v8
