// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --js-staging
// Flags: --experimental-wasm-rab-integration

(function TestUnsharedTransitions() {
  let mem = new WebAssembly.Memory({initial: 1, maximum: 2});
  let ab0 = mem.buffer;
  assertFalse(ab0.resizable);
  let ab1 = mem.toResizableBuffer();
  assertTrue(ab1.resizable);
  assertFalse(ab0 === ab1);
  assertTrue(ab0.detached);
  assertFalse(ab1.detached);
  let ab2 = mem.toFixedLengthBuffer();
  assertFalse(ab2.resizable);
  assertFalse(ab1 === ab2);
  assertFalse(ab0 === ab2);
  assertTrue(ab0.detached);
  assertTrue(ab1.detached);
  assertFalse(ab2.detached);
})();

(function TestSharedTransitions() {
  let mem = new WebAssembly.Memory({initial: 1, maximum: 2, shared: true});
  let ab0 = mem.buffer;
  assertFalse(ab0.growable);
  let ab1 = mem.toResizableBuffer();
  assertTrue(ab1.growable);
  assertFalse(ab0 === ab1);
  let ab2 = mem.toFixedLengthBuffer();
  assertFalse(ab2.growable);
  assertFalse(ab1 === ab2);
  assertFalse(ab0 === ab2);
})();
