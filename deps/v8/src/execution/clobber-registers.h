// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_CLOBBER_REGISTERS_H_
#define V8_EXECUTION_CLOBBER_REGISTERS_H_

namespace v8 {

namespace internal {

double ClobberDoubleRegisters(double x1, double x2, double x3, double x4);

}
}  // namespace v8

#endif  // V8_EXECUTION_CLOBBER_REGISTERS_H_
