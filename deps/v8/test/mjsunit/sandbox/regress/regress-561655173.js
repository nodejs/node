// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/sandbox/wasm-jspi.js');

function get_resume(promise) {
  let promise_ptr = getPtr(promise);
  let reaction = getField(promise_ptr, kJSPromiseReactionsOrResultOffset);
  return Sandbox.getObjectAt(
      getField(reaction, kPromiseReactionFulfillHandlerOffset));
}

let pending;
let builder = new WasmModuleBuilder();
let imp_idx = builder.addImport('m', 's', kSig_v_v);
builder.addFunction('f', kSig_v_v)
    .addBody([kExprCallFunction, imp_idx])
    .exportFunc();
let instance = builder.instantiate({
  m: {s: new WebAssembly.Suspending(() => pending = new Promise(() => {}))}
});
let call = WebAssembly.promising(instance.exports.f);
call();

let resume = get_resume(pending);
let rhs = {[Symbol.hasInstance]: resume.bind(null, 0x21212121, 0x22222222)};
let unused = 0x21212121 instanceof rhs;

assertUnreachable('Over-applied WasmResume call should trap');
