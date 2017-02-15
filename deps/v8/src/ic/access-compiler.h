// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_ACCESS_COMPILER_H_
#define V8_IC_ACCESS_COMPILER_H_

#include "src/code-stubs.h"
#include "src/macro-assembler.h"
#include "src/objects.h"

namespace v8 {
namespace internal {


class PropertyAccessCompiler BASE_EMBEDDED {
 public:
  static Builtins::Name MissBuiltin(Code::Kind kind) {
    switch (kind) {
      case Code::LOAD_IC:
        return Builtins::kLoadIC_Miss;
      case Code::STORE_IC:
        return Builtins::kStoreIC_Miss;
      case Code::KEYED_LOAD_IC:
        return Builtins::kKeyedLoadIC_Miss;
      case Code::KEYED_STORE_IC:
        return Builtins::kKeyedStoreIC_Miss;
      default:
        UNREACHABLE();
    }
    return Builtins::kLoadIC_Miss;
  }

  static void TailCallBuiltin(MacroAssembler* masm, Builtins::Name name);

 protected:
  PropertyAccessCompiler(Isolate* isolate, Code::Kind kind,
                         CacheHolderFlag cache_holder)
      : registers_(GetCallingConvention(kind)),
        kind_(kind),
        cache_holder_(cache_holder),
        isolate_(isolate),
        masm_(isolate, NULL, 256, CodeObjectRequired::kYes) {
    // TODO(yangguo): remove this once we can serialize IC stubs.
    masm_.enable_serializer();
  }

  Code::Kind kind() const { return kind_; }
  CacheHolderFlag cache_holder() const { return cache_holder_; }
  MacroAssembler* masm() { return &masm_; }
  Isolate* isolate() const { return isolate_; }
  Heap* heap() const { return isolate()->heap(); }
  Factory* factory() const { return isolate()->factory(); }

  Register receiver() const { return registers_[0]; }
  Register name() const { return registers_[1]; }
  Register slot() const;
  Register vector() const;
  Register scratch1() const { return registers_[2]; }
  Register scratch2() const { return registers_[3]; }

  static Register* GetCallingConvention(Code::Kind);
  static Register* load_calling_convention();
  static Register* store_calling_convention();
  static Register* keyed_store_calling_convention();

  Register* registers_;

  static void GenerateTailCall(MacroAssembler* masm, Handle<Code> code);

  Handle<Code> GetCodeWithFlags(Code::Flags flags, const char* name);
  Handle<Code> GetCodeWithFlags(Code::Flags flags, Handle<Name> name);

 private:
  Code::Kind kind_;
  CacheHolderFlag cache_holder_;

  Isolate* isolate_;
  MacroAssembler masm_;
  // Ensure that MacroAssembler has a reasonable size.
  STATIC_ASSERT(sizeof(MacroAssembler) < 128 * kPointerSize);
};
}  // namespace internal
}  // namespace v8

#endif  // V8_IC_ACCESS_COMPILER_H_
