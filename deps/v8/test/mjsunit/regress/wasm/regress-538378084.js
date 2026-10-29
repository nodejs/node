// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-max-code-space-size-mb=456 --expose-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const kMaxReservation = 838912;

const instr = SimdInstr(kExprI32x4TruncSatF64x2UZero);
const chain = n => {
  const b = [kExprLocalGet, 0, ...SimdInstr(kExprI32x4Splat)];
  for (let i = 0; i < n; i++) b.push(...instr);
  b.push(...SimdInstr(kExprI32x4ExtractLane), 0);
  return b;
};

const builder = new WasmModuleBuilder();
let chain_6k = chain(6000);
builder.addFunction('big', kSig_i_i).addBody(chain(18300)).exportFunc();
builder.addFunction('small', kSig_i_i).addBody(chain_6k).exportFunc();
// Never called, so never compiled. These only inflate the initial reservation,
// via EstimateNativeModuleCodeSize = 3*body+24 per declared function, which is
// where the free slack for {small} comes from.
for (let i = 0; i < 24; i++) {
  builder.addFunction('pad' + i, kSig_i_i).addBody(chain_6k).exportFunc();
}
const wire = builder.toBuffer();

// Only 'big' is tiered up, so only it serializes as real code; everything else
// stays a 1-byte marker and is compiled after deserialization.
let module = new WebAssembly.Module(wire);
let inst0 = new WebAssembly.Instance(module);
inst0.exports.big(3);
%WasmTierUpFunction(inst0.exports.big);
const blob = d8.wasm.serializeModule(module);
// We want to achieve blob.byteLength > kMaxReservation here, but can't
// assert it because in the no-AVX configuration the generated code is smaller.
// That's fine, the default config provides enough coverage.

// This is load bearing for some reason.
print("" + blob.byteLength);

// Clear the NativeModuleCache to force real deserialization.
module = null; inst0 = null;
gc();

const inst = new WebAssembly.Instance(d8.wasm.deserializeModule(blob, wire));
assertEquals(0, inst.exports.big(3));
assertEquals(0, inst.exports.small(5));
assertEquals(0, inst.exports.big(3));
