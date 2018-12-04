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
IC_BUILTIN(LoadIC_Megamorphic)
IC_BUILTIN(LoadIC_Noninlined)
IC_BUILTIN(LoadIC_Uninitialized)
IC_BUILTIN(LoadICTrampoline)
IC_BUILTIN(LoadICTrampoline_Megamorphic)
IC_BUILTIN(KeyedLoadIC)
IC_BUILTIN(KeyedLoadIC_Megamorphic)
IC_BUILTIN(KeyedLoadIC_PolymorphicName)
IC_BUILTIN(KeyedLoadICTrampoline)
IC_BUILTIN(KeyedLoadICTrampoline_Megamorphic)
IC_BUILTIN(StoreGlobalIC)
IC_BUILTIN(StoreGlobalICTrampoline)
IC_BUILTIN(StoreIC)
IC_BUILTIN(StoreICTrampoline)
IC_BUILTIN(KeyedStoreIC)
IC_BUILTIN(KeyedStoreICTrampoline)
IC_BUILTIN(StoreInArrayLiteralIC)
IC_BUILTIN(CloneObjectIC)
IC_BUILTIN(CloneObjectIC_Slow)

IC_BUILTIN_PARAM(LoadGlobalIC, LoadGlobalIC, NOT_INSIDE_TYPEOF)
IC_BUILTIN_PARAM(LoadGlobalICInsideTypeof, LoadGlobalIC, INSIDE_TYPEOF)
IC_BUILTIN_PARAM(LoadGlobalICTrampoline, LoadGlobalICTrampoline,
                 NOT_INSIDE_TYPEOF)
IC_BUILTIN_PARAM(LoadGlobalICInsideTypeofTrampoline, LoadGlobalICTrampoline,
                 INSIDE_TYPEOF)

}  // namespace internal
}  // namespace v8
