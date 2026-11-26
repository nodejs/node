// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-stack-trace-frames --wasm-test-streaming

Error.prepareStackTrace = (e, s) => s[0].getFunction();

assertPromiseResult(
    WebAssembly.compileStreaming(17), assertUnreachable,
    e => {
      const compileCallback = e.stack;
      assertEquals(undefined, compileCallback);
    });
