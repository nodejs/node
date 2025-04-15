// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: differential_fuzz/fake_resource.js
print("I'm a resource.");

// Original: differential_fuzz/fake_resource.js
print("I'm a resource.");

// Original: resources/async_iterator.js
try {
  AsyncIterator;
} catch (e) {
  this.AsyncIterator = class AsyncIterator extends Iterator {};
}

// Original: regress/iterator/input.js
let __v_0 = 5;
try {
  AsyncIterator.foo(__v_0);
} catch (e) {}
