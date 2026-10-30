// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --verify-heap --conservative-stack-scanning --regexp-interpret-all

function workerCode() {
  function overflow() {
    try {
      overflow();
      throw 1;
    } catch (e) {
      try {
        %NewRegExpWithBacktrackLimit(".+".repeat(99), "", 1).exec();
      } catch (e) {}
    }
  }
  try {
    performance.measureMemory();
  } catch (e) {}
  try {
    Object.defineProperty(d8.__proto__, "then", {
      get: overflow
    });
  } catch (e) {}
}

assertEquals(undefined, new Worker(workerCode, { type: "function" }).getMessage());
