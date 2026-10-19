// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wasm-generic-wrapper --stack-size=100

d8.file.execute('test/mjsunit/mjsunit.js');
d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function exhaustAndCall(callFn, expectOverflow = true) {
  let maxDepth = 0;
  let depth = 0;
  let didCall = false;
  let caughtError = null;
  function recurse() {
    depth++;
    if (depth > maxDepth) maxDepth = depth;
    try {
      recurse();
    } catch (e) {
      depth--;
      // Unwind 10 frames to provide controlled headroom (~800 bytes): enough
      // for a 0-slot wrapper to run successfully, but small enough to trigger
      // overflow when allocating large parameter or return frames.
      if (depth === maxDepth - 10 && !didCall) {
        didCall = true;
        try {
          callFn();
        } catch (callErr) {
          caughtError = callErr;
        }
      }
      throw e;
    }
  }
  assertThrows(recurse, RangeError, /Maximum call stack size exceeded/);
  assertTrue(didCall);
  if (expectOverflow) {
    assertInstanceof(caughtError, RangeError);
    assertEquals('Maximum call stack size exceeded', caughtError.message);
  } else {
    assertEquals(null, caughtError);
  }
}

(function testCompiledWrapperPreemptiveStackCheckParams() {
  let builder = new WasmModuleBuilder();
  let sig = builder.addType(makeSig(Array(1000).fill(kWasmI32), []));
  builder.addFunction('main', sig).exportFunc().addBody([]);
  let instance = builder.instantiate();

  // Warm up before stack exhaustion.
  instance.exports.main();

  // Call without arguments: missing parameters are default-initialized, so no
  // JS argument slots are pushed. The wrapper is entered with minimal stack
  // usage, and its prologue checks that the stack can hold the 1,000 Wasm
  // parameter slots.
  exhaustAndCall(instance.exports.main);
})();

(function testCompiledWrapperPreemptiveStackCheckReturns() {
  let builder = new WasmModuleBuilder();
  let sig = builder.addType(makeSig([], Array(1000).fill(kWasmI32)));
  let body = [];
  for (let i = 0; i < 1000; ++i) {
    body.push(kExprI32Const, 0);
  }
  builder.addFunction('main', sig).exportFunc().addBody(body);
  let instance = builder.instantiate();

  // Warm up before stack exhaustion.
  instance.exports.main();

  // Multi-value returns exceeding register capacity allocate return slots
  // on the wrapper stack frame, exercising the stack check for return counts.
  exhaustAndCall(instance.exports.main);
})();

(function testCompiledWrapperPreemptiveStackCheckZeroSlots() {
  let builder = new WasmModuleBuilder();
  let sig = builder.addType(makeSig([], []));
  builder.addFunction('main', sig).exportFunc().addBody([]);
  let instance = builder.instantiate();

  // Warm up before stack exhaustion.
  instance.exports.main();

  // A 0-slot wrapper requires no extra stack space beyond its fixed frame,
  // so it should execute and succeed within the headroom of the unwound frame.
  exhaustAndCall(instance.exports.main, false);
})();
