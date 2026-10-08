// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-test-streaming --wasm-max-module-size=100

let limit = 100;
let buf = new Uint8Array(limit + 1);
buf.set([0, 0x61, 0x73, 0x6d, 1]);  // Header
let payloadLen = limit - 8 - 1 - 5;
let v = payloadLen;
for (let i = 0; i < 5; i++) {
  buf[9 + i] =v & 0x7f | 0x80;
  v >>= 7;
}
buf[13] &= 0x7f;

function ErrorChecker(error) {
  assertEquals("WebAssembly.compileStreaming(): "+
               "size > maximum module size (100): 101 @+0", error.message);
}
assertPromiseResult(WebAssembly.compileStreaming(Promise.resolve(buf))
                          .then(assertUnreachable, ErrorChecker));
