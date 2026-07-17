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
// Original: fuzzilli/fuzzdir-diff-fuzz-2/corpus/program_3.js
try {
  boom;
} catch (__v_0) {}
try {
  boom;
} catch (__v_1) {
  __prettyPrintExtra(__v_1);
}
try {
  boom;
} catch (__v_2) {
  __caught++;
}
try {
  boom;
} catch (__v_3) {}
try {
  print("Hash: " + __hash);
  print("Caught: " + __caught);
} catch (e) {}
