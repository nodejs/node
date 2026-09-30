// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// Regression test for GC-safe ref conversion in CallExternalJSFunction.
// Without the two-pass fix, fresh funcrefs requiring wrapper allocation can
// trigger GC while refs are stored in the non-GC-scanned CWasmArgumentsPacker
// buffer, leading to stale pointers and crashes.
//
// The vulnerability requires:
// 1. Import with ≥2 ref-typed parameters (input path) or returns (return path)
// 2. Call with freshly-allocated funcrefs whose JS wrappers have not
//    materialized
// 3. The first conversion allocates and triggers GC
// 4. Result: previously-packed refs become stale, identity is lost
//
// This test triggers the vulnerable pattern (externref + funcref requiring
// allocation) many times and explicitly validates ref identity through GC.
// Without the two-pass fix, it crashes with AllowHeapAllocation check failure
// when WasmToJSObject allocates during the packing loop.

(function TestRefIdentityThroughGC() {
  const builder = new WasmModuleBuilder();
  const importSig = makeSig([kWasmExternRef, kWasmFuncRef], [kWasmExternRef]);
  const callerSig = makeSig([kWasmExternRef], [kWasmExternRef]);
  const targetSig = makeSig([], []);

  const importIndex = builder.addImport('env', 'mix', importSig);
  const targets = [];
  for (let i = 0; i < 64; i++) {
    targets.push(
      builder.addFunction(`target_${i}`, targetSig).addBody([]).index
    );
  }

  for (let i = 0; i < targets.length; i++) {
    builder
      .addFunction(`call_${i}`, callerSig)
      .addBody([
        kExprLocalGet, 0,
        kExprRefFunc, targets[i],
        kExprCallFunction, importIndex,
      ])
      .exportFunc();
  }

  builder.addDeclarativeElementSegment(targets);

  let calls = 0;
  const marker = {
    tag: 'first-packed-ref',
    payload: new Array(256).fill(0x41414141),
  };
  const instance = builder.instantiate({
    env: {
      mix(first, func) {
        calls++;
        if (first !== marker) {
          throw new Error(
            `first ref identity corrupted before JS import at call ${calls}`
          );
        }
        if (typeof func !== 'function') {
          throw new Error(
            `funcref did not convert to JS function at call ${calls}`
          );
        }
        // Allocate aggressively to trigger GC during PASS 1 conversions
        for (let i = 0; i < 128; i++) new Array(128).fill(i);
        if (typeof gc === 'function') gc();
        if (first !== marker) {
          throw new Error(
            `first ref identity corrupted after GC at call ${calls}`
          );
        }
        return first;
      }
    }
  });

  // Repeat the vulnerable pattern to trigger GC and test ref stability.
  // Without the fix, this crashes with AllowHeapAllocation check failure.
  for (let round = 0; round < 10; round++) {
    for (let i = 0; i < Math.min(targets.length, 16); i++) {
      const result = instance.exports[`call_${i}`](marker);
      if (result !== marker) {
        throw new Error(
          `returned ref identity corrupted at round ${round}, function ${i}`
        );
      }
    }
  }

  console.log('TestRefIdentityThroughGC passed');
})();

// Regression test for packer.offset_ accounting in PASS 2 of
// return-value handling.
// When refs and primitives are interleaved in return signatures, the
// packer offset
// must advance correctly even when refs come from ref_returns container.
// Without the fix, a ref followed by a primitive causes the
// primitive's Pop<>()
// to read garbage (the first bytes of the ref slot).
(function TestMixedReturnSignatureOffsetAccounting() {
  const builder = new WasmModuleBuilder();
  // Return signature: (externref, i32) to exercise offset accounting.
  const importSig = makeSig(
      [kWasmExternRef, kWasmI32],
      [kWasmExternRef, kWasmI32]
  );
  const callerSig = makeSig(
    [kWasmExternRef, kWasmI32],
    [kWasmExternRef, kWasmI32]
  );

  const importIndex = builder.addImport('env', 'echoMixed', importSig);

  builder
    .addFunction('caller', callerSig)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprCallFunction, importIndex,
    ])
    .exportFunc();

  let calls = 0;
  const marker = {tag: 'mixed-return-test'};
  const instance = builder.instantiate({
    env: {
      echoMixed(externRef, i32Val) {
        calls++;
        if (externRef !== marker) {
          throw new Error(`externRef identity corrupted at call ${calls}`);
        }
        // The i32 must be returned correctly despite packer offset tracking
        // Without the fix, this would be corrupted.
        return [externRef, i32Val];
      }
    }
  });

  // Test with a few key i32 values to catch offset corruption.
  const testValues = [0xDEADBEEF, -1];
  for (let i = 0; i < testValues.length; i++) {
    const testVal = testValues[i];
    const [refResult, i32Result] = instance.exports.caller(marker, testVal);
    if (refResult !== marker) {
      throw new Error(`externRef corrupted for testVal=${testVal}`);
    }
    // When passing 0xDEADBEEF (3735928559) as i32, it is interpreted as
    // signed (-559038737). The return should match either the signed or
    // unsigned interpretation depending on
    // how the value round-tripped through JS.
    // Convert to signed 32-bit to match Wasm i32 interpretation.
    const expectedResult = (testVal | 0);
    if (i32Result !== expectedResult) {
      throw new Error(
        `i32 value corrupted: expected ${expectedResult}, got ${i32Result} ` +
        `(likely packer offset bug in PASS 2 ref handling)`
      );
    }
  }

  console.log('TestMixedReturnSignatureOffsetAccounting passed');
})();
