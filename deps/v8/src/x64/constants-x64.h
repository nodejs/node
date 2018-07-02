// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_X64_CONSTANTS_X64_H_
#define V8_X64_CONSTANTS_X64_H_

// Actual value of root register is offset from the root array's start
// to take advantage of negative displacement values.
// TODO(sigurds): Choose best value.
constexpr int kRootRegisterBias = 128;

#endif  // V8_X64_CONSTANTS_X64_H_
