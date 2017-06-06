// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/ic/accessor-assembler.h"

namespace v8 {
namespace internal {

#define IC_BUILTIN(Name)                                                \
  void Builtins::Generate_##Name(compiler::CodeAssemblerState* state) { \
    AccessorAssembler assembler(state);                                 \
    assembler.Generate##Name();                                         \
  }

#define IC_BUILTIN_PARAM(BuiltinName, GeneratorName, parameter)                \
  void Builtins::Generate_##BuiltinName(compiler::CodeAssemblerState* state) { \
    AccessorAssembler assembler(state);                                        \
    assembler.Generate##GeneratorName(parameter);                              \
  }

IC_BUILTIN(LoadIC)
IC_BUILTIN(LoadIC_Noninlined)
IC_BUILTIN(LoadIC_Uninitialized)
IC_BUILTIN(KeyedLoadIC)
IC_BUILTIN(LoadICTrampoline)
IC_BUILTIN(LoadField)
IC_BUILTIN(KeyedLoadICTrampoline)
IC_BUILTIN(KeyedLoadIC_Megamorphic)

IC_BUILTIN_PARAM(StoreIC, StoreIC, SLOPPY)
IC_BUILTIN_PARAM(StoreICTrampoline, StoreICTrampoline, SLOPPY)
IC_BUILTIN_PARAM(StoreICStrict, StoreIC, STRICT)
IC_BUILTIN_PARAM(StoreICStrictTrampoline, StoreICTrampoline, STRICT)
IC_BUILTIN_PARAM(KeyedStoreIC, KeyedStoreIC, SLOPPY)
IC_BUILTIN_PARAM(KeyedStoreICTrampoline, KeyedStoreICTrampoline, SLOPPY)
IC_BUILTIN_PARAM(KeyedStoreICStrict, KeyedStoreIC, STRICT)
IC_BUILTIN_PARAM(KeyedStoreICStrictTrampoline, KeyedStoreICTrampoline, STRICT)
IC_BUILTIN_PARAM(LoadGlobalIC, LoadGlobalIC, NOT_INSIDE_TYPEOF)
IC_BUILTIN_PARAM(LoadGlobalICInsideTypeof, LoadGlobalIC, INSIDE_TYPEOF)
IC_BUILTIN_PARAM(LoadGlobalICTrampoline, LoadGlobalICTrampoline,
                 NOT_INSIDE_TYPEOF)
IC_BUILTIN_PARAM(LoadGlobalICInsideTypeofTrampoline, LoadGlobalICTrampoline,
                 INSIDE_TYPEOF)
IC_BUILTIN_PARAM(LoadICProtoArray, LoadICProtoArray, false)
IC_BUILTIN_PARAM(LoadICProtoArrayThrowIfNonexistent, LoadICProtoArray, true)

}  // namespace internal
}  // namespace v8
