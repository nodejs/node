// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --trace-wasm-globals

function module(global, imports, buffer) {
  'use asm';
  var g = imports.g | 0;

  function f() {}
  return {f: f};
}

module(globalThis, {});
