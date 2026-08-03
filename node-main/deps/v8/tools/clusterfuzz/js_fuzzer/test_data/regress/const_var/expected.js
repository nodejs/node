// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: differential_fuzz/fake_resource.js
print("I'm a resource.");

// Original: differential_fuzz/fake_resource.js
print("I'm a resource.");

// Original: regress/const_var/input.js
const __v_0 = 0;
const __v_1 = 0;
const __v_2 = 0;
const __v_3 = 0;
let __v_4 = 0;
let __v_5 = 0;
try {
  __v_5 = /* VariableMutator: Replaced __v_0 with __v_4 */__v_4;
} catch (e) {}
try {
  __v_5 = /* VariableMutator: Replaced __v_1 with __v_0 */__v_0;
} catch (e) {}
try {
  /* VariableMutator: Replaced __v_5 with __v_4 */__v_4 = /* VariableMutator: Replaced __v_2 with __v_3 */__v_3;
} catch (e) {}
try {
  /* VariableMutator: Replaced __v_5 with __v_4 */__v_4 = /* VariableMutator: Replaced __v_3 with __v_2 */__v_2;
} catch (e) {}
try {
  /* VariableMutator: Replaced __v_5 with __v_4 */__v_4 = /* VariableMutator: Replaced __v_4 with __v_3 */__v_3;
} catch (e) {}
try {
  __v_4 = __v_0;
} catch (e) {}
try {
  __v_4 = /* VariableMutator: Replaced __v_1 with __v_3 */__v_3;
} catch (e) {}
try {
  /* VariableMutator: Replaced __v_4 with __v_5 */__v_5 = /* VariableMutator: Replaced __v_2 with __v_4 */__v_4;
} catch (e) {}
try {
  __v_4 = /* VariableMutator: Replaced __v_3 with __v_0 */__v_0;
} catch (e) {}
try {
  __v_4 = /* VariableMutator: Replaced __v_5 with __v_2 */__v_2;
} catch (e) {}
