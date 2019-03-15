// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8.h"

#include "src/code-desc.h"
#include "src/handles-inl.h"
#include "src/isolate.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace test_factory {

TEST(Factory_NewCode) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  HandleScope scope(i_isolate);

  // Create a big function that ends up in CODE_LO_SPACE.
  const int instruction_size = kMaxRegularHeapObjectSize + 1;
  std::unique_ptr<byte[]> instructions(new byte[instruction_size]);

  CodeDesc desc;
  desc.buffer = instructions.get();
  desc.buffer_size = instruction_size;
  desc.instr_size = instruction_size;
  desc.reloc_size = 0;
  desc.constant_pool_size = 0;
  desc.unwinding_info = nullptr;
  desc.unwinding_info_size = 0;
  desc.origin = nullptr;
  Handle<Object> self_ref;
  Handle<Code> code =
      i_isolate->factory()->NewCode(desc, Code::WASM_FUNCTION, self_ref);

  CHECK(i_isolate->heap()->InSpace(*code, CODE_LO_SPACE));
#if VERIFY_HEAP
  code->ObjectVerify(i_isolate);
#endif
}

}  // namespace test_factory
}  // namespace internal
}  // namespace v8
