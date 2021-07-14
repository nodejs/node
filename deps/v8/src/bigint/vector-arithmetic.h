// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper functions that operate on {Digits} vectors of digits.

#ifndef V8_BIGINT_VECTOR_ARITHMETIC_H_
#define V8_BIGINT_VECTOR_ARITHMETIC_H_

#include "src/bigint/bigint.h"

namespace v8 {
namespace bigint {

inline bool IsDigitNormalized(Digits X) { return X.len() == 0 || X.msd() != 0; }

}  // namespace bigint
}  // namespace v8

#endif  // V8_BIGINT_VECTOR_ARITHMETIC_H_
