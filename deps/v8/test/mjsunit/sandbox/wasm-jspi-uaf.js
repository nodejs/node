// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/sandbox/wasm-jspi.js');

let builder = new WasmModuleBuilder();
let resolve0;
let promises = [new Promise(r => { resolve0 = r; }),
                new Promise(r => setTimeout(r, 0))];
let suspend0 = new WebAssembly.Suspending(() => promises[0]);
let suspend0_index = builder.addImport("m", "suspend0", kSig_v_v);
let suspend1 = new WebAssembly.Suspending(() => promises[1]);
let suspend1_index = builder.addImport("m", "suspend1", kSig_v_v);
function corrupt() {
  set_suspender(
      get_resume_data(promises[0]),
      get_suspender(get_resume_data(promises[1])));
}
let corrupt_index = builder.addImport("m", "corrupt", kSig_v_v);
builder.addExport("suspend0", suspend0_index);
builder.addExport("suspend1", suspend1_index);
builder.addExport("corrupt", corrupt_index);
let instance = builder.instantiate({m: {suspend0, suspend1, corrupt}});

WebAssembly.promising(instance.exports.suspend0)();
WebAssembly.promising(instance.exports.suspend1)()
  .then(v => {
    gc({type:'major', execution:'sync', flavor:'last-resort'});
    resolve0();
  });
instance.exports.corrupt();
