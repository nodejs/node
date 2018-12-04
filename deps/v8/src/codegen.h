// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_H_
#define V8_CODEGEN_H_

namespace v8 {
namespace internal {

// Results of the library implementation of transcendental functions may differ
// from the one we use in our generated code.  Therefore we use the same
// generated code both in runtime and compiled code.
typedef double (*UnaryMathFunction)(double x);

UnaryMathFunction CreateSqrtFunction();

// Custom implementation of math functions.
double fast_sqrt(double input);
void lazily_initialize_fast_sqrt();

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_H_
