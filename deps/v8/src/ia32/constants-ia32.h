// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IA32_CONSTANTS_IA32_H_
#define V8_IA32_CONSTANTS_IA32_H_

// Actual value of root register is offset from the root array's start
// to take advantage of negative displacement values.
// For x86, this value is provided for uniformity with other platforms, although
// currently no root register is present.
constexpr int kRootRegisterBias = 0;

#endif  // V8_IA32_CONSTANTS_IA32_H_
