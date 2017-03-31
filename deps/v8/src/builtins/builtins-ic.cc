// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/ic/accessor-assembler.h"

namespace v8 {
namespace internal {

TF_BUILTIN(LoadIC, CodeStubAssembler) {
  AccessorAssembler::GenerateLoadIC(state());
}

TF_BUILTIN(KeyedLoadIC, CodeStubAssembler) {
  AccessorAssembler::GenerateKeyedLoadICTF(state());
}

TF_BUILTIN(LoadICTrampoline, CodeStubAssembler) {
  AccessorAssembler::GenerateLoadICTrampoline(state());
}

TF_BUILTIN(KeyedLoadICTrampoline, CodeStubAssembler) {
  AccessorAssembler::GenerateKeyedLoadICTrampolineTF(state());
}

TF_BUILTIN(StoreIC, CodeStubAssembler) {
  AccessorAssembler::GenerateStoreIC(state());
}

TF_BUILTIN(StoreICTrampoline, CodeStubAssembler) {
  AccessorAssembler::GenerateStoreICTrampoline(state());
}

TF_BUILTIN(StoreICStrict, CodeStubAssembler) {
  AccessorAssembler::GenerateStoreIC(state());
}

TF_BUILTIN(StoreICStrictTrampoline, CodeStubAssembler) {
  AccessorAssembler::GenerateStoreICTrampoline(state());
}

TF_BUILTIN(KeyedStoreIC, CodeStubAssembler) {
  AccessorAssembler::GenerateKeyedStoreICTF(state(), SLOPPY);
}

TF_BUILTIN(KeyedStoreICTrampoline, CodeStubAssembler) {
  AccessorAssembler::GenerateKeyedStoreICTrampolineTF(state(), SLOPPY);
}

TF_BUILTIN(KeyedStoreICStrict, CodeStubAssembler) {
  AccessorAssembler::GenerateKeyedStoreICTF(state(), STRICT);
}

TF_BUILTIN(KeyedStoreICStrictTrampoline, CodeStubAssembler) {
  AccessorAssembler::GenerateKeyedStoreICTrampolineTF(state(), STRICT);
}

TF_BUILTIN(LoadGlobalIC, CodeStubAssembler) {
  AccessorAssembler::GenerateLoadGlobalIC(state(), NOT_INSIDE_TYPEOF);
}

TF_BUILTIN(LoadGlobalICInsideTypeof, CodeStubAssembler) {
  AccessorAssembler::GenerateLoadGlobalIC(state(), INSIDE_TYPEOF);
}

TF_BUILTIN(LoadGlobalICTrampoline, CodeStubAssembler) {
  AccessorAssembler::GenerateLoadGlobalICTrampoline(state(), NOT_INSIDE_TYPEOF);
}

TF_BUILTIN(LoadGlobalICInsideTypeofTrampoline, CodeStubAssembler) {
  AccessorAssembler::GenerateLoadGlobalICTrampoline(state(), INSIDE_TYPEOF);
}

}  // namespace internal
}  // namespace v8
