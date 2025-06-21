// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: differential_fuzz/fake_resource.js
print("I'm a resource.");

// Original: differential_fuzz/fake_resource.js
print("I'm a resource.");

/* DifferentialFuzzMutator: Print variables and exceptions from section */
try {
  print("Hash: " + __hash);
  print("Caught: " + __caught);
} catch (e) {}
print("v8-foozzie source: fuzzilli_source");
// Original: fuzzilli/fuzzdir-2/corpus/program_2.js
let __v_0 = 0;
if (Math.random() > 0.5) {
  let __v_1 = "";
  __v_0++;
  if (__v_0) {
    const __v_2 = 0;
    /* DifferentialFuzzMutator: Extra variable printing */
    __prettyPrintExtra(__v_2);
    __v_1 = __v_2 + "";
    __v_1 = __v_2 + "";
    /* DifferentialFuzzMutator: Extra variable printing */
    __prettyPrintExtra(__v_1);
    __v_1 = __v_2 + "";
    __v_0 = __v_1;
    __v_0 = __v_1;
    /* DifferentialFuzzMutator: Extra variable printing */
    __prettyPrintExtra(__v_0);
    __v_0 = __v_1;
  }
}
/* DifferentialFuzzMutator: Print variables and exceptions from section */
try {
  print("Hash: " + __hash);
  print("Caught: " + __caught);
  __prettyPrint(__v_0);
} catch (e) {}
