// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wasm-generic-wrapper --stack-size=200

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function exhaustAndCall(callFn, checkError = true, unwindFrames = 10) {
  let maxDepth = 0;
  let depth = 0;
  let didCall = false;
  let caughtError = null;
  let result;
  function recurse() {
    depth++;
    if (depth > maxDepth) maxDepth = depth;
    try {
      recurse();
    } catch (e) {
      depth--;
      // Unwind unwindFrames frames: enough to enter the JS-to-Wasm wrapper and
      // Wasm function, reaching the compiled Wasm-to-JS wrapper where outgoing
      // parameters are checked.
      if (depth === maxDepth - unwindFrames && !didCall) {
        didCall = true;
        try {
          result = callFn(0);
        } catch (callErr) {
          caughtError = callErr;
        }
      }
      throw e;
    }
  }
  assertThrows(recurse, RangeError, /Maximum call stack size exceeded/);
  assertTrue(didCall);
  if (checkError) {
    assertInstanceof(caughtError, RangeError);
    assertEquals('Maximum call stack size exceeded', caughtError.message);
    assertMatches(/wasm-function/, caughtError.stack);
  } else {
    assertNull(caughtError);
  }
  return result;
}

const kHighArity = 14000;
const paramNames = Array.from({length: kHighArity}, (_, i) => `p${i}`);
const highArityImportFn = new Function(paramNames.join(','), 'return 1;');

(function testHighExpectedArityJSImport() {
  const builder = new WasmModuleBuilder();
  const sig = builder.addType(kSig_i_i);
  const impIndex = builder.addImport('env', 'f', sig);
  builder.addFunction('main', sig)
      .addBody([kExprLocalGet, 0, kExprCallFunction, impIndex])
      .exportFunc();
  const instance = builder.instantiate({env: {f: highArityImportFn}});

  // Pushing 14,001 slots (~112 KB on 64-bit) exceeds the stack limit and margin
  // when called near stack exhaustion.
  exhaustAndCall(instance.exports.main);
})();

(function testManyWasmParamsImport() {
  const kNumParams = 1000;
  const builder = new WasmModuleBuilder();
  const sig = builder.addType(
      makeSig(Array(kNumParams).fill(kWasmExternRef), [kWasmI32]));
  const impIndex = builder.addImport('env', 'f', sig);
  const body = [];
  for (let i = 0; i < kNumParams; ++i) {
    body.push(kExprRefNull, kExternRefCode);
  }
  body.push(kExprCallFunction, impIndex);
  builder.addFunction('main', kSig_i_v).addBody(body).exportFunc();
  const instance = builder.instantiate({env: {f: () => 42}});

  assertEquals(42, instance.exports.main());
  exhaustAndCall(instance.exports.main);
})();

(function testJSPIHighExpectedArityImport() {
  if (typeof WebAssembly.promising !== 'function') return;

  const suspendingImport = new WebAssembly.Suspending(highArityImportFn);

  const builder = new WasmModuleBuilder();
  const sig = builder.addType(kSig_i_i);
  const impIndex = builder.addImport('env', 'f', sig);
  builder.addFunction('main', sig)
      .addBody([kExprLocalGet, 0, kExprCallFunction, impIndex])
      .exportFunc();
  const instance = builder.instantiate({env: {f: suspendingImport}});
  const promisingMain = WebAssembly.promising(instance.exports.main);

  // For JSPI, allocating a suspender on the central stack requires enough stack
  // headroom for GC during stress modes, so unwind more frames (50). The
  // parameter payload (14,000 slots / ~112 KB) still comfortably exceeds the
  // remaining stack headroom on a 200 KB stack.
  const promise = exhaustAndCall(promisingMain, false, 50);
  assertPromiseResult(promise, assertUnreachable, err => {
    assertInstanceof(err, RangeError);
    assertMatches(/wasm-function/, err.stack);
  });
})();
