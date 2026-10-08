// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-revectorize

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.addMemory(16, 16);
builder.addType(makeSig([], []));
let sig = builder.addType(makeSig([kWasmI32], []));

let N = 20;

let func = builder.addFunction("test", sig)
  .addLocals(kWasmS128, N + 10)
  .exportFunc();

let body = [];

// param1 is local 0.
let temp1 = 1;
let temp2 = 2;
let temp3 = 3;
let dag_start = 4;

// temp1 = I16x8Splat(10)
body.push(
  kExprI32Const, 10,
  kSimdPrefix, ...wasmUnsignedLeb(kExprI16x8Splat),
  kExprLocalSet, temp1
);

// temp2 = I16x8Neg(I16x8Abs(I16x8SConvertI8x16Low(temp1)))
body.push(
  kExprLocalGet, temp1,
  kSimdPrefix, ...wasmUnsignedLeb(kExprI16x8SConvertI8x16Low),
  kSimdPrefix, ...wasmUnsignedLeb(kExprI16x8Abs),
  kSimdPrefix, ...wasmUnsignedLeb(kExprI16x8Neg),
  kExprLocalSet, temp2
);

// temp3 = I8x16ReplaceLane(temp1, 1)
body.push(
  kExprLocalGet, temp1,
  kExprI32Const, 1,
  kSimdPrefix, ...wasmUnsignedLeb(kExprI8x16ReplaceLane), 2,
  kExprLocalSet, temp3
);

let curr = temp3;
for (let i = 0; i < N; i++) {
  let next = dag_start + i;
  body.push(
    kExprLocalGet, curr,
    kExprLocalGet, curr,
    kExprLocalGet, curr,
    kSimdPrefix, kExprS128Select,
    kExprLocalSet, next
  );
  curr = next;
}
let temp4_input = curr;
let temp4 = dag_start + N;

// temp4 = I16x8Neg(I16x8Abs(I16x8SConvertI8x16Low(temp4_input)))
body.push(
  kExprLocalGet, temp4_input,
  kSimdPrefix, ...wasmUnsignedLeb(kExprI16x8SConvertI8x16Low),
  kSimdPrefix, ...wasmUnsignedLeb(kExprI16x8Abs),
  kSimdPrefix, ...wasmUnsignedLeb(kExprI16x8Neg),
  kExprLocalSet, temp4
);

// Store temp4 and temp2 adjacently to force pack them.
body.push(
  kExprLocalGet, 0, // param1
  kExprLocalGet, temp4, // The later one
  kSimdPrefix, ...wasmUnsignedLeb(kExprS128StoreMem), 0, 0,

  kExprLocalGet, 0, // param1
  kExprLocalGet, temp2, // The earlier one
  kSimdPrefix, ...wasmUnsignedLeb(kExprS128StoreMem), 0, 16,
);

func.addBody(body);

let instance = builder.instantiate();
instance.exports.test(0);

// The chain of S128Select nodes above forms a diamond-shaped DAG: each level
// reuses the same predecessor as all 3 of its inputs. Revectorization's
// ReduceInputsOfOp() does a BFS over the input tree rooted at the packed
// store nodes; if it doesn't dedupe an OpIndex before (rather than after)
// enqueueing it, that DAG causes an exponential (3^N) blowup and hangs.
%WasmTierUpFunction(instance.exports.test);
instance.exports.test(0);
