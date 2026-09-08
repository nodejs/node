// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-deopt --allow-natives-syntax --liftoff
// Flags: --wasm-inlining --wasm-inlining-ignore-call-counts --no-jit-fuzzing
// Flags: --wasm-stack-switching-stack-size=32

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// Triggers a wasm deopt close to the stack limit on a JSPI secondary stack.
// The optimized function speculatively inlines a chain of functions that each
// declare many v128 locals, so the materialised Liftoff output frames are much
// larger than the optimized frame. The function-entry stack check has to
// account for this so that the output frames still fit on the stack.
(function TestDeoptLargeOutputFramesNearStackLimit() {
  print(arguments.callee.name);
  const kLocalCount = 480;
  const kChainLength = 8;

  let builder = new WasmModuleBuilder();
  let funcRefT = builder.addType(kSig_i_v);
  let table = builder.addTable(wasmRefNullType(funcRefT), kChainLength + 2);

  // The chain f0..f7: each function declares many v128 locals (so the Liftoff
  // frame is large) and performs a call_ref through the table to the next
  // function in the chain. With monomorphic feedback this whole chain gets
  // speculatively inlined into f0.
  let chain = [];
  for (let i = 0; i < kChainLength; ++i) {
    chain.push(
        builder.addFunction("f" + i, funcRefT)
          .addLocals(kWasmS128, kLocalCount)
          .addBody([
            kExprI32Const, i + 1,
            kExprTableGet, table.index,
            kExprCallRef, funcRefT,
          ]));
  }
  chain[0].exportFunc();
  let leafA = builder.addFunction("leafA", funcRefT)
                 .addBody([kExprI32Const, 1]);
  let leafB = builder.addFunction("leafB", funcRefT)
                 .addBody([kExprI32Const, 2]);

  // Initially every table slot points at the next function in the chain and
  // the slot after the last chain function points at {leafA}.
  let initialTargets = [];
  for (let i = 0; i < kChainLength; ++i) {
    initialTargets.push([kExprRefFunc, chain[i].index]);
  }
  initialTargets.push([kExprRefFunc, leafA.index]);
  initialTargets.push([kExprRefFunc, leafB.index]);
  builder.addActiveElementSegment(table.index, wasmI32Const(0), initialTargets,
                                  wasmRefNullType(funcRefT));

  // The chain entry is called through a separate table so that it does not get
  // inlined into {recur}.
  let mainTable = builder.addTable(wasmRefNullType(funcRefT), 1);
  builder.addActiveElementSegment(mainTable.index, wasmI32Const(0),
                                  [[kExprRefFunc, chain[0].index]],
                                  wasmRefNullType(funcRefT));

  // Recurse {depth} times with small frames, then optionally call the chain.
  let recur = builder.addFunction("recur", kSig_i_ii);
  recur.addBody([
    kExprLocalGet, 0,
    kExprIf, kWasmI32,
      kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub,
      kExprLocalGet, 1,
      kExprCallFunction, recur.index,
    kExprElse,
      kExprLocalGet, 1,
      kExprIf, kWasmI32,
        kExprI32Const, 0,
        kExprCallIndirect, funcRefT, mainTable.index,
      kExprElse,
        kExprI32Const, 0,
      kExprEnd,
    kExprEnd,
  ]).exportFunc();

  builder.addFunction("setLeaf", kSig_v_i)
    .addBody([
      kExprI32Const, kChainLength - 1,
      kExprLocalGet, 0,
      kExprTableGet, table.index,
      kExprTableSet, table.index,
    ]).exportFunc();

  let wasm = builder.instantiate().exports;

  // Collect monomorphic feedback for every call_ref in the chain.
  assertEquals(1, wasm.f0());
  assertEquals(1, wasm.f0());

  %WasmTierUpFunction(wasm.f0);
  if (%IsWasmTieringPredictable()) {
    assertTrue(%IsTurboFanFunction(wasm.f0));
  }
  assertEquals(1, wasm.f0());

  let promisingRecur = WebAssembly.promising(wasm.recur);

  // Find the largest recursion depth at which {recur} still fits on the JSPI
  // stack without calling into the chain.
  let lo = 0, hi = 64;
  function probe(depth) {
    return promisingRecur(depth, 0).then(_ => true, e => {
      assertInstanceof(e, RangeError);
      return false;
    });
  }
  let chainPromise = probe(hi);
  function expandHi(ok) {
    if (!ok) return;
    lo = hi; hi *= 2;
    return probe(hi).then(expandHi);
  }
  function bisect() {
    if (hi - lo <= 1) return;
    let mid = (lo + hi) >> 1;
    return probe(mid).then(ok => {
      if (ok) lo = mid; else hi = mid;
      return bisect();
    });
  }
  chainPromise = chainPromise.then(expandHi).then(bisect).then(() => {
    // Redirect the deepest speculatively-inlined call_ref to a different
    // target so the next call deopts.
    wasm.setLeaf(kChainLength + 1);
    if (%IsWasmTieringPredictable()) {
      assertTrue(%IsTurboFanFunction(wasm.f0));
    }
    // Walk from the stack limit upwards and call the optimized chain at each
    // depth. As long as the stack does not have enough room for the deopt
    // output frames the call must throw a RangeError; once enough room is
    // available the deopt happens normally and the call returns.
    let depth = lo;
    function step() {
      if (depth < 0) return;
      let d = depth;
      depth -= 16;
      return promisingRecur(d, 1).then(
          v => { assertEquals(2, v); },
          e => { assertInstanceof(e, RangeError); return step(); });
    }
    return step();
  });
  assertPromiseResult(chainPromise);
})();
