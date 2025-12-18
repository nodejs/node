// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/sandbox/wasm-jspi.js');

let builder = new WasmModuleBuilder();
let promises = [new Promise(r => setTimeout(r, 0)),
                new Promise(r => setTimeout(r, 0))];
let suspend = new WebAssembly.Suspending(i => promises[i]);
let suspend_index = builder.addImport("m", "suspend", kSig_v_i);
builder.addFunction("main", kSig_v_i).addBody([
    kExprLocalGet, 0,
    kExprCallFunction, suspend_index,
]).exportFunc();
let instance = builder.instantiate({m:{suspend}});

let promise_all = Promise.all([
    WebAssembly.promising(instance.exports.main)(0),
    WebAssembly.promising(instance.exports.main)(1)]);

// Overwrite the first suspender with the second. The second stack will be
// resumed instead of the first stack, without any crashes at first.
// Then, when the second promise is actually fulfilled, it attempts to resume
// the same stack again but the stack is already retired, which crashes safely.
set_suspender(
    get_resume_data(promises[0]),
    get_suspender(get_resume_data(promises[1])));

promise_all.then(() => {
  assertUnreachable("Process should have been killed.");
});
