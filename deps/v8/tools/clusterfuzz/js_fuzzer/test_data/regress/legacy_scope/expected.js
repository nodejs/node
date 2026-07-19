// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: differential_fuzz/fake_resource.js
print("I'm a resource.");

// Original: differential_fuzz/fake_resource.js
print("I'm a resource.");

// Original: regress/legacy_scope/input.js
function __f_0() {
  function __f_1() {}
  try {
    var __f_1 = 42;
  } catch (e) {}
}
