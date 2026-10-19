// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const fn = builder.addImport('m', 'fn', kSig_v_v);
builder.addFunction('main', kSig_v_v)
    .addBody([kExprCallFunction, fn])
    .exportFunc();
const instance = builder.instantiate({m: {fn: Math.sin}});

let objectId;
let paused_count = 0;
function receive(message) {
  const msg = JSON.parse(message);
  if (msg.id === 2) {
    objectId = msg.result.result.objectId;
  } else if (msg.method === 'Debugger.paused') {
    ++paused_count;
    send(JSON.stringify({id: 3 + paused_count, method: 'Debugger.stepInto'}));
  }
}

send(JSON.stringify({id: 1, method: 'Debugger.enable'}));
send(JSON.stringify({
  id: 2,
  method: 'Runtime.evaluate',
  params: {expression: 'Math.sin'},
}));
send(JSON.stringify({
  id: 3,
  method: 'Debugger.setBreakpointOnFunctionCall',
  params: {objectId},
}));

instance.exports.main();
assertEquals(3, paused_count);
