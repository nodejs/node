// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/builtins/builtins-utils.h"

namespace v8 {
namespace internal {

void Builtins::Generate_IteratorPrototypeIterator(
    CodeStubAssembler* assembler) {
  assembler->Return(assembler->Parameter(0));
}

}  // namespace internal
}  // namespace v8
