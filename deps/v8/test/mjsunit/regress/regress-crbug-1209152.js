// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --multi-mapped-mock-allocator

let size = 8 * 1024 * 1024;
let initialized = 2 * 1024 * 1008;
let array = new Uint8Array(size);
for (let i = 0; i < initialized; i++) {
  array[i] = 42;
}
array.sort();
