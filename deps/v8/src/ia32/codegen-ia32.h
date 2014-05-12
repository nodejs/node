// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IA32_CODEGEN_IA32_H_
#define V8_IA32_CODEGEN_IA32_H_

#include "ast.h"
#include "ic-inl.h"

namespace v8 {
namespace internal {


class StringCharLoadGenerator : public AllStatic {
 public:
  // Generates the code for handling different string types and loading the
  // indexed character into |result|.  We expect |index| as untagged input and
  // |result| as untagged output.
  static void Generate(MacroAssembler* masm,
                       Factory* factory,
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
                          XMMRegister input,
                          XMMRegister result,
                          XMMRegister double_scratch,
                          Register temp1,
                          Register temp2);

 private:
  DISALLOW_COPY_AND_ASSIGN(MathExpGenerator);
};

} }  // namespace v8::internal

#endif  // V8_IA32_CODEGEN_IA32_H_
