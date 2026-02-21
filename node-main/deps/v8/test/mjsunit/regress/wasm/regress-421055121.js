// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-growable-stacks --stack-size=100

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let instance;

function f() {
  instance.exports.stack_overflow();
}

let builder = new WasmModuleBuilder();
let import_index = builder.addImport("imports", "f", kSig_v_v);
let stack_overflow = builder.addFunction("stack_overflow", kSig_v_v);
stack_overflow.addBody([
    kExprCallFunction, stack_overflow.index
]).exportFunc();
builder.addFunction("main", kSig_v_v).addBody([
    kExprCallFunction, import_index,
]).exportFunc();

instance = builder.instantiate({ imports: { f } });
assertPromiseResult(
    WebAssembly.promising(instance.exports.main)(),
    v => assertUnreachable(),
    v => assertInstanceof(v, RangeError));
