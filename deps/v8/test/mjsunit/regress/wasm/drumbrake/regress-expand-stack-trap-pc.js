// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless --jitless

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Mutually recursive functions with large frames, so that the interpreter
// stack cannot be expanded when entering the callee. The trap must not compute
// a pc for the callee from the caller's bytecode.
const builder = new WasmModuleBuilder();
const f = builder.addFunction('f', kSig_i_i).addLocals(kWasmI64, 49000);
const g = builder.addFunction('g', kSig_i_i).addLocals(kWasmI64, 49000);
f.addBody([
  kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add,
  kExprCallFunction, g.index
]).exportFunc();
g.addBody([
  kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add,
  kExprCallFunction, f.index
]).exportFunc();
const instance = builder.instantiate();

// Both entry points, since the two bytecode buffers can be in either order.
assertThrows(() => instance.exports.f(0), RangeError);
assertThrows(() => instance.exports.g(0), RangeError);
