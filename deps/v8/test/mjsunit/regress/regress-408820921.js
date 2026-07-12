// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function g() {
  function f() {
    try {
      f();
    } catch (e) {
      %NewRegExpWithBacktrackLimit(".+".repeat(99), "", 1).exec();
    }
  }
  performance.measureMemory();
  Object.defineProperty(d8.__proto__, "then", { get: f });
}

// Shouldn't crash:
assertEquals(undefined, (new Worker(g, { type: "function" })).getMessage());
