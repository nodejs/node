// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: differential_fuzz/fake_resource.js
print("I'm a resource.");

// Original: differential_fuzz/fake_resource.js
print("I'm a resource.");

// Original: regress/yield/input.js
function* __f_0() {
  try {
    let __v_0 = yield 1;
    let __v_1 = 3 + (yield 2);
  } catch (e) {}
}
