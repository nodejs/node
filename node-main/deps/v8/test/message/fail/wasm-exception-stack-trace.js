// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
let tag = new WebAssembly.Tag({parameters: []});
let import_index = builder.addImport("m", "throw", kSig_v_v);
builder.addFunction("throw", kSig_v_v)
    .addBody([
      kExprCallFunction, import_index
]).exportFunc();
function throw_wasm_exn() {
  let exception = new WebAssembly.Exception(tag, [], {traceStack: true});
  throw exception;
}
let instance = builder.instantiate({"m": {"throw": throw_wasm_exn}});
instance.exports.throw();
