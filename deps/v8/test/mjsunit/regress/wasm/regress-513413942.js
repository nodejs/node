// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector --allow-natives-syntax --no-wasm-tier-up
// Flags: --no-wasm-trap-handler --stress-wasm-memory-moving

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let wasmScriptId = null;
let instance = null;

globalThis.receive = function(m) {
  let msg = JSON.parse(m);
  if (msg.method === 'Debugger.scriptParsed') {
    if (msg.params.url && msg.params.url.startsWith('wasm://')) {
      wasmScriptId = msg.params.scriptId;
    }
  }
};

globalThis.handleInspectorMessage = function() {
  send(JSON.stringify({id: 100, method: 'Debugger.resume'}));
};

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1000);
builder.exportMemoryAs('memory', 0);

const body = [
  ...wasmI32Const(0),
  kExprI32LoadMem, 0, 0,
  kExprDrop,
  kExprNop,
  ...wasmI32Const(0),
  ...wasmI32Const(123),
  kExprI32StoreMem, 0, 0,
];

const target = builder.addFunction('target', kSig_v_v)
  .addBody(body)
  .exportFunc();

const module_bytes = builder.toBuffer();
const nop_offset = target.body_offset + 6;

send(JSON.stringify({id: 1, method: 'Debugger.enable'}));
const module = new WebAssembly.Module(module_bytes);
instance = new WebAssembly.Instance(module);
globalThis.instance = instance;
globalThis.grown = false;

const condition = `(function() {
  if (!globalThis.grown) {
    globalThis.instance.exports.memory.grow(1);
    globalThis.grown = true;
    return true;
  }
  return false;
})()`;

send(JSON.stringify({
  id: 2,
  method: 'Debugger.setBreakpoint',
  params: {
    location: {scriptId: wasmScriptId, lineNumber: 0, columnNumber: nop_offset},
    condition: condition
  }
}));

instance.exports.target();
