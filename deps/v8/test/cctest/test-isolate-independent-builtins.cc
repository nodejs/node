// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/cctest.h"

#include "src/assembler.h"
#include "src/handles-inl.h"
#include "src/isolate.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {
namespace test_isolate_independent_builtins {

TEST(VerifyBuiltinsIsolateIndependence) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handle_scope(isolate);

  // Build a white-list of all isolate-independent RelocInfo entry kinds.
  constexpr int all_real_modes_mask =
      (1 << (RelocInfo::LAST_REAL_RELOC_MODE + 1)) - 1;
  constexpr int mode_mask =
      all_real_modes_mask & ~RelocInfo::ModeMask(RelocInfo::COMMENT) &
      ~RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) &
      ~RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED) &
      ~RelocInfo::ModeMask(RelocInfo::CONST_POOL) &
      ~RelocInfo::ModeMask(RelocInfo::VENEER_POOL);
  STATIC_ASSERT(RelocInfo::LAST_REAL_RELOC_MODE == RelocInfo::VENEER_POOL);
  STATIC_ASSERT(RelocInfo::ModeMask(RelocInfo::COMMENT) ==
                (1 << RelocInfo::COMMENT));
  STATIC_ASSERT(
      mode_mask ==
      (RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
       RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
       RelocInfo::ModeMask(RelocInfo::WASM_CONTEXT_REFERENCE) |
       RelocInfo::ModeMask(RelocInfo::WASM_FUNCTION_TABLE_SIZE_REFERENCE) |
       RelocInfo::ModeMask(RelocInfo::WASM_GLOBAL_HANDLE) |
       RelocInfo::ModeMask(RelocInfo::WASM_CALL) |
       RelocInfo::ModeMask(RelocInfo::JS_TO_WASM_CALL) |
       RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY) |
       RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE)));

  constexpr bool kVerbose = false;
  bool found_mismatch = false;
  for (int i = 0; i < Builtins::builtin_count; i++) {
    Code* code = isolate->builtins()->builtin(i);

    // Make sure the builtin is deserialized.
    if (code->builtin_index() == Builtins::kDeserializeLazy &&
        Builtins::IsLazy(i)) {
      code = Snapshot::DeserializeBuiltin(isolate, i);
    }

    if (kVerbose) {
      printf("%s %s\n", Builtins::KindNameOf(i), isolate->builtins()->name(i));
    }

    bool is_isolate_independent = true;
    for (RelocIterator it(code, mode_mask); !it.done(); it.next()) {
      is_isolate_independent = false;

#ifdef ENABLE_DISASSEMBLER
      if (kVerbose) {
        RelocInfo::Mode mode = it.rinfo()->rmode();
        printf("  %s\n", RelocInfo::RelocModeName(mode));
      }
#endif
    }

    const bool expected_result = Builtins::IsIsolateIndependent(i);
    if (is_isolate_independent != expected_result) {
      found_mismatch = true;
      printf("%s %s expected: %d, is: %d\n", Builtins::KindNameOf(i),
             isolate->builtins()->name(i), expected_result,
             is_isolate_independent);
    }
  }

  CHECK(!found_mismatch);
}

}  // namespace test_isolate_independent_builtins
}  // namespace internal
}  // namespace v8
