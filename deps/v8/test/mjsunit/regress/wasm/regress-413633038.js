// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

function c0() {
  return %WasmArray();
}

class c1 extends c0 {
  h = 1;
}

assertThrows(() => new c1(), TypeError, /WebAssembly objects are opaque/);
