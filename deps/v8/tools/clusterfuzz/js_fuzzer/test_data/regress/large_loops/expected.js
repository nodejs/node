// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: differential_fuzz/fake_resource.js
print("I'm a resource.");

// Original: differential_fuzz/fake_resource.js
print("I'm a resource.");

// Original: regress/large_loops/input.js
var __v_0 = 0;
var __v_1 = 0;
for (const __v_2 = 0; __v_2 < 10000; __v_2++) {
  console.log();
}
for (const __v_3 = 0; 1e5 >= __v_3; __v_3--) {
  console.log();
}
for (const __v_4 = -10000; __v_4 < 0; __v_4++) {
  console.log();
  /* VariableOrObjectMutator: Random mutation */
  __v_4[__getRandomProperty(__v_4, 758323)] = __v_4;
  if (__v_4 != null && typeof __v_4 == "object") Object.defineProperty(__v_4, __getRandomProperty(__v_4, 947075), {
    value: -9007199254740990
  });
}
for (const __v_5 = 0n; __v_5 < 10000n; __v_5++) {
  console.log();
  /* VariableOrObjectMutator: Random mutation */
  delete __v_5[__getRandomProperty(__v_5, 81149)];
}
for (const __v_6 = -0.3; __v_6 < 1000.1; __v_6 += 0.5) {
  console.log();
}
