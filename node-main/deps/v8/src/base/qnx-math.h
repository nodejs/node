// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_QNX_MATH_H_
#define V8_BASE_QNX_MATH_H_

#include <cmath>

#undef fpclassify
#undef isfinite
#undef isinf
#undef isnan
#undef isnormal
#undef signbit

using std::lrint;

#endif  // V8_BASE_QNX_MATH_H_
