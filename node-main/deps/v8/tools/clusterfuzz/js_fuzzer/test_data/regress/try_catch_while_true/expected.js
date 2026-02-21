// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: differential_fuzz/fake_resource.js
print("I'm a resource.");

// Original: differential_fuzz/fake_resource.js
print("I'm a resource.");

// Original: regress/try_catch_while_true/input.js
try {
  while (true) {
    if (cond) throw Exception();
    console.log("forever");
  }
} catch (e) {}
try {
  do {
    if (cond) throw Exception();
    console.log("forever");
  } while (true);
} catch (e) {}
