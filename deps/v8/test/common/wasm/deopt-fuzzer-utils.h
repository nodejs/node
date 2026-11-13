// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_COMMON_WASM_DEOPT_FUZZER_UTILS_H_
#define V8_TEST_COMMON_WASM_DEOPT_FUZZER_UTILS_H_

#include <cstdint>

#include "include/v8-forward.h"
#include "src/base/vector.h"

// Helpers shared by the libFuzzer-style deopt fuzzer
// (test/fuzzer/wasm/deopt.cc) and its FuzzTest port
// (test/unittests/wasm/deopt-fuzztest.cc).
namespace v8::internal::wasm::fuzzing {

// Sets the V8 flags needed by the deopt fuzzer. Only the first call has an
// effect.
void ConfigureDeoptFuzzerFlags(v8::Isolate* isolate);

// This function contains the core logic of the deopt fuzzer. It fuzzes deopts,
// and returns a libfuzzer return code indicating whether the input should be
// added to the corpus. It returns -1 if an input should not be added to the
// corpus, and 0 if an input is fine to add to the corpus.
//
// It generates a main function accepting a call target. The call target is then
// used in a call_ref or call_indirect. The fuzzer runs the program in a
// reference run to collect expected results.
// Then it performs the same run on a new module optimizing the module after
// every target, causing emission of deopt nodes and potentially triggering
// deopts. Note that if the code containing the speculative call is unreachable
// or not inlined, the fuzzer won't generate a deopt node and won't perform a
// deopt.
//
// Pseudo code of a minimal wasm module that the fuzzer could generate:
//
// int global0 = 0;
// Table table = [callee0, callee1];
//
// int callee0(int a, int b) {
//   return a + b;
// }
//
// int callee1(int a, int b) {
//   return a * b;
// }
//
// int inlinee(int a, int b) {
//   auto callee = table.get(global0);
//   return call_ref(auto_callee)(a, b);
// }
//
// int main(int callee_index) {
//   global0 = callee_index;
//   return inlinee(1, 2);
// }
//
// The fuzzer then performs the following test:
//   assertEquals(expected_val0, main(0)); // Collects feedback.
//   %WasmTierUpFunction(main);
//   assertEquals(expected_val1, main(1)); // Potentially triggers deopt.
int FuzzWasmDeopt(base::Vector<const uint8_t> data, v8::Isolate* isolate);

}  // namespace v8::internal::wasm::fuzzing

#endif  // V8_TEST_COMMON_WASM_DEOPT_FUZZER_UTILS_H_
