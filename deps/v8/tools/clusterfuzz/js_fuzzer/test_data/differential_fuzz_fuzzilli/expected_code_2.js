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
// Original: fuzzilli/fuzzdir-1/corpus/program_3.js
let __v_0 = {
  a: 1,
  b: 2
};
/* DifferentialFuzzMutator: Extra variable printing */
__prettyPrintExtra(__v_0);
const __v_1 = 29234234234234;
function __f_0(__v_2) {
  let __v_3 = __v_2 + 5;
  let __v_4 = dummy;
  let __v_5 = dummy;
  try {
    __v_2++;
    __v_4 = __v_5;
    /* DifferentialFuzzMutator: Extra variable printing */
    __prettyPrintExtra(__v_4);
    __v_5.prop = {};
    __v_5.prop = {};
    /* DifferentialFuzzMutator: Extra variable printing */
    __prettyPrintExtra(__v_5);
    __v_5.prop = {};
    __v_5.prop = {};
    __v_5.prop = {};
  } catch (__v_6) {
    __v_2 = __v_3;
    /* DifferentialFuzzMutator: Extra variable printing */
    __prettyPrintExtra(__v_2);
  }
  return {
    a: __v_2,
    b: __v_2,
    c: __v_0
  };
}
%PrepareFunctionForOptimization(__f_0);
__f_0(__v_1);
%OptimizeFunctionOnNextCall(__f_0);
__f_0(__v_0.a);
/* DifferentialFuzzMutator: Print variables and exceptions from section */
try {
  print("Hash: " + __hash);
  print("Caught: " + __caught);
  __prettyPrint(__v_0);
  __prettyPrint(__v_1);
} catch (e) {}
