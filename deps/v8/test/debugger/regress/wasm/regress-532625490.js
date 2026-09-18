// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector --fuzzing

// Trigger an error in the inspector 'receive' callback during BindToCurrentContext.
// BindToCurrentContext is triggered by script compilation (e.g. eval).
// This should not hit the DCHECK(AllowExceptions::IsAllowed(this)) in Isolate::ReportPendingMessages.
globalThis.receive = function(message) {
  throw new Error("fuzzer error");
};

send(JSON.stringify({
  id: 0,
  method: 'Debugger.enable',
}));

eval("1 + 1");

// Also test Wasm instantiation which triggered the original report.
d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
const builder = new WasmModuleBuilder();
builder.addFunction('main', kSig_v_v).addBody([]).exportFunc();
builder.instantiate();
