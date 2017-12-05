// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_ACCESS_COMPILER_H_
#define V8_IC_ACCESS_COMPILER_H_

#include "src/code-stubs.h"
#include "src/ic/access-compiler-data.h"
#include "src/macro-assembler.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

class PropertyAccessCompiler BASE_EMBEDDED {
 public:
  enum Type { LOAD, STORE };

  static void TailCallBuiltin(MacroAssembler* masm, Builtins::Name name);

 protected:
  PropertyAccessCompiler(Isolate* isolate, Type type)
      : registers_(GetCallingConvention(isolate, type)),
        type_(type),
        isolate_(isolate),
        masm_(isolate, NULL, 256, CodeObjectRequired::kYes) {
    // TODO(yangguo): remove this once we can serialize IC stubs.
    masm_.enable_serializer();
  }

  Type type() const { return type_; }

  MacroAssembler* masm() { return &masm_; }
  Isolate* isolate() const { return isolate_; }
  Factory* factory() const { return isolate()->factory(); }

  Register receiver() const { return registers_[0]; }
  Register name() const { return registers_[1]; }
  Register slot() const;
  Register vector() const;
  Register scratch1() const { return registers_[2]; }
  Register scratch2() const { return registers_[3]; }

  Register* registers_;

  static void GenerateTailCall(MacroAssembler* masm, Handle<Code> code);

 private:
  static Register* GetCallingConvention(Isolate* isolate, Type type);
  static void InitializePlatformSpecific(AccessCompilerData* data);

  Type type_;
  Isolate* isolate_;
  MacroAssembler masm_;
  // Ensure that MacroAssembler has a reasonable size.
  STATIC_ASSERT(sizeof(MacroAssembler) < 128 * kPointerSize);
};
}  // namespace internal
}  // namespace v8

#endif  // V8_IC_ACCESS_COMPILER_H_
