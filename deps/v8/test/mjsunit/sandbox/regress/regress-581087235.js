// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const $struct = builder.addStruct([makeField(kWasmI64, true)]);
const $sig_i_l = builder.addType(kSig_i_l);
const $sig_l_l = builder.addType(kSig_l_l);
const $sig_s_l = builder.addType(makeSig([kWasmI64], [wasmRefType($struct)]));
const $sig_i31_l =
    builder.addType(makeSig([kWasmI64], [wasmRefType(kWasmI31Ref)]));
const $sig_v_ll = builder.addType(makeSig([kWasmI64, kWasmI64], []));

const $extract = builder.addImport('imports', 'extract', $sig_i31_l);

const $numericSource = builder.addFunction('numericSource', $sig_l_l)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
    ]);

const $numericTarget = builder.addFunction('numericTarget', $sig_i_l)
    .exportFunc()
    .addBody([
      ...new Array(700).fill(kExprNop),
      kExprLocalGet, 0,
      kExprI32ConvertI64,
    ]);

const $g_num = builder.addGlobal(
    wasmRefType($sig_i_l), false, false,
    [kExprRefFunc, $numericTarget.index]);

const $bridge = builder.addFunction('bridge', $sig_i_l)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprGlobalGet, $g_num.index,
      kExprCallRef, $sig_i_l,
    ]);

const $warmExtract = builder.addFunction('warmExtract', $sig_i_l)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprCallFunction, $extract,
      kGCPrefix, kExprI31GetS,
    ]);

const $typedRefTarget = builder.addFunction('typedRefTarget', $sig_s_l)
    .exportFunc()
    .addBody([
      ...wasmI64Const(0),
      kGCPrefix, kExprStructNew, $struct,
    ]);

const $g_ref = builder.addGlobal(
    wasmRefType($sig_s_l), false, false,
    [kExprRefFunc, $typedRefTarget.index]);

const $write = builder.addFunction('write', $sig_v_ll)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprGlobalGet, $g_ref.index,
      kExprCallRef, $sig_s_l,
      kExprLocalGet, 1,
      kGCPrefix, kExprStructSet, $struct, 0,
    ]);

builder.addExport('typedRefSource', $extract);

let e;
function extract(x) {
  return e.bridge(x) / 2;
}
e = builder.instantiate({imports: {extract}}).exports;

%PrepareFunctionForOptimization(extract);
for (let i = 0; i < 10; ++i) {
  e.warmExtract(BigInt(i * 2));
}
%OptimizeFunctionOnNextCall(extract);
e.warmExtract(0n);

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const bufferOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('WASM_GLOBAL_OBJECT_TYPE'), 'buffer');
const internalOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('WASM_FUNC_REF_TYPE'), 'trusted_internal');

function refOf(fn) {
  const global = new WebAssembly.Global({value: 'anyfunc'}, fn);
  const buffer =
      memory.getUint32(Sandbox.getAddressOf(global) + bufferOffset, true) & ~1;
  return memory.getUint32(buffer + 8, true) & ~1;
}

function swap(target, source) {
  const targetRef = refOf(target);
  memory.setUint32(
      targetRef + internalOffset,
      memory.getUint32(refOf(source) + internalOffset, true), true);
}

swap(e.numericTarget, e.numericSource);
swap(e.typedRefTarget, e.typedRefSource);

const address = 0x10000000000n;
e.write(address, 0x1122334455667788n);
