// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_CODEGEN_ARM64_H_
#define V8_ARM64_CODEGEN_ARM64_H_

#include "src/ast.h"
#include "src/ic-inl.h"

namespace v8 {
namespace internal {

class StringCharLoadGenerator : public AllStatic {
 public:
  // Generates the code for handling different string types and loading the
  // indexed character into |result|.  We expect |index| as untagged input and
  // |result| as untagged output. Register index is asserted to be a 32-bit W
  // register.
  static void Generate(MacroAssembler* masm,
                       Register string,
                       Register index,
                       Register result,
                       Label* call_runtime);

 private:
  DISALLOW_COPY_AND_ASSIGN(StringCharLoadGenerator);
};


class MathExpGenerator : public AllStatic {
 public:
  static void EmitMathExp(MacroAssembler* masm,
                          DoubleRegister input,
                          DoubleRegister result,
                          DoubleRegister double_scratch1,
                          DoubleRegister double_scratch2,
                          Register temp1,
                          Register temp2,
                          Register temp3);

 private:
  DISALLOW_COPY_AND_ASSIGN(MathExpGenerator);
};

} }  // namespace v8::internal

#endif  // V8_ARM64_CODEGEN_ARM64_H_
