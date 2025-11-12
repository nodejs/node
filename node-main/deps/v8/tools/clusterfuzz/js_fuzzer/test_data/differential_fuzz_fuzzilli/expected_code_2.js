// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: differential_fuzz/fake_resource.js
print("I'm a resource.");

// Original: differential_fuzz/fake_resource.js
print("I'm a resource.");

try {
  print("Hash: " + __hash);
  print("Caught: " + __caught);
} catch (e) {}
print("v8-foozzie source: fuzzilli_diff_fuzz_source");
// Original: fuzzilli/fuzzdir-diff-fuzz-1/corpus/program_1.js
console.log(42);
try {
  print("Hash: " + __hash);
  print("Caught: " + __caught);
} catch (e) {}
