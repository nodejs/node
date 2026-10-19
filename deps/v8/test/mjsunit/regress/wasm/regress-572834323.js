// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

send(JSON.stringify({id: 1, method: 'Debugger.enable'}));
send(JSON.stringify({
  id: 2,
  method: 'Debugger.setInstrumentationBreakpoint',
  params: {instrumentation: 'beforeScriptExecution'},
}));

const builder = new WasmModuleBuilder();
builder.addImport('m', 'f', kSig_v_v);
const module = builder.toModule();

assertThrows(
    () => new WebAssembly.Instance(module, {m: {f: 123}}),
    WebAssembly.LinkError);
