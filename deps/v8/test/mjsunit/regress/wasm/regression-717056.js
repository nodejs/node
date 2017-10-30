// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Check that stack overflow inside asm-wasm translation propagates correctly.

function asm() {
  'use asm';
  return {};
}

function rec() {
  asm();
  rec();
}
assertThrows(() => rec(), RangeError);
