// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_X87_CODEGEN_X87_H_
#define V8_X87_CODEGEN_X87_H_

#include "src/ast/ast.h"
#include "src/macro-assembler.h"

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

}  // namespace internal
}  // namespace v8

#endif  // V8_X87_CODEGEN_X87_H_
