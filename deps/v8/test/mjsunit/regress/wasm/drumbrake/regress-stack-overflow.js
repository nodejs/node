// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless --jitless --stack-size=100

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('zeroSlots', kSig_v_v).addBody([]).exportFunc();
builder.addFunction('manyParams', makeSig(Array(1000).fill(kWasmI32), []))
    .addBody([])
    .exportFunc();
builder.addFunction('manyReturns', makeSig([], Array(1000).fill(kWasmI32)))
    .addBody(Array(1000).fill([kExprI32Const, 0]).flat())
    .exportFunc();
const instance = builder.instantiate();

function exhaustAndCall(fn, unwindFrames = 0, warmUp = true) {
  if (warmUp) fn();
  let depth = 0;
  let maxDepth = 0;
  let didCall = false;
  let caughtError;
  function recurse() {
    depth++;
    maxDepth = Math.max(maxDepth, depth);
    try {
      recurse();
    } catch (error) {
      depth--;
      if (depth === maxDepth - unwindFrames - 1 && !didCall) {
        didCall = true;
        try {
          fn();
        } catch (callError) {
          caughtError = callError;
        }
      }
      throw error;
    }
  }
  assertThrows(recurse, RangeError, /Maximum call stack size exceeded/);
  assertTrue(didCall);
  assertInstanceof(caughtError, RangeError);
  assertEquals('Maximum call stack size exceeded', caughtError.message);
  fn();
}

for (const fn of Object.values(instance.exports)) {
  exhaustAndCall(fn);
}
exhaustAndCall(instance.exports.manyReturns, 10);
exhaustAndCall(builder.instantiate().exports.zeroSlots, 0, false);
exhaustAndCall(builder.instantiate().exports.manyReturns, 10, false);

const kParamCount = 1000;
// Wasm-to-JS: an import with the maximum number of parameters makes the
// wrapper reserve a large argument area on the native stack.
const importBuilder = new WasmModuleBuilder();
const importIndex = importBuilder.addImport(
    'env', 'f', makeSig(Array(kParamCount).fill(kWasmI32), [kWasmI32]));
importBuilder.addFunction('callImport', kSig_i_v)
    .addBody([
      ...Array(kParamCount).fill([kExprI32Const, 0]).flat(),
      kExprCallFunction, importIndex,
    ])
    .exportFunc();
const importInstance = importBuilder.instantiate({env: {f: () => 42}});
const callImport = importInstance.exports.callImport;
assertEquals(42, callImport());

// Call the export at each of the last frames before the stack limit. A call
// must either succeed or throw a RangeError.
const kSweptFrames = 300;
let importSweptFrames = 0;
let importOverflowed = 0;
let importSucceeded = 0;
let importUnexpected = 0;
function recurseImport() {
  try {
    recurseImport();
  } catch (e) {
    if (importSweptFrames++ < kSweptFrames) {
      // Avoid calling helpers here: there is hardly any stack left.
      try {
        if (callImport() === 42) importSucceeded++; else importUnexpected++;
      } catch (callError) {
        if (callError instanceof RangeError) {
          importOverflowed++;
        } else {
          importUnexpected++;
        }
      }
    }
    throw e;
  }
}
assertThrows(recurseImport, RangeError, /Maximum call stack size exceeded/);
assertEquals(0, importUnexpected);
assertTrue(importOverflowed > 0);
assertTrue(importSucceeded > 0);
assertEquals(42, callImport());
