// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector

'use strict';

let wasmScriptId = null;
let lastBreakpointId = null;
let scriptIds = [];

function receive(m) {
  let msg = JSON.parse(m);
  if (msg.method === 'Debugger.scriptParsed') {
    if (msg.params.url.startsWith('wasm://')) {
      wasmScriptId = msg.params.scriptId;
      print('Wasm script parsed: ' + wasmScriptId);
    }
    scriptIds.push(msg.params.scriptId);
  }
  if (msg.id === 2) {
    lastBreakpointId = msg.result.breakpointId;
  }
}

function handleInspectorMessage() {
  print('Paused!');

  // 5) Remove BP at Y (offset 34)
  // This will call UpdateReturnAddresses with a new code that (buggy)
  // might not have a source position for Y.
  if (lastBreakpointId) {
    print('Removing breakpoint at Y: ' + lastBreakpointId);
    send(JSON.stringify({
      id: 10,
      method: 'Debugger.removeBreakpoint',
      params: {breakpointId: lastBreakpointId}
    }));
  }

  // 6) Set BP at X (offset 37)
  // This would be the next step in the two-isolate PoC, but we might have
  // already crashed.
  print('Setting breakpoint at X...');
  send(JSON.stringify({
    id: 11,
    method: 'Debugger.setBreakpoint',
    params:
        {location: {scriptId: wasmScriptId, lineNumber: 0, columnNumber: 37}}
  }));

  // 8) Resume
  print('Resuming...');
  send(JSON.stringify({id: 12, method: 'Debugger.resume'}));
}

// Enable debugger
send(JSON.stringify({id: 1, method: 'Debugger.enable'}));

const bytes = new Uint8Array([
  0x00,0x61,0x73,0x6d, 0x01,0x00,0x00,0x00,
  0x01,0x06, 0x01, 0x60,0x01,0x7f,0x01,0x7f,
  0x03,0x02, 0x01, 0x00,
  0x07,0x05, 0x01, 0x01,0x66, 0x00,0x00,
  0x0a,0x12, 0x01,
    0x10, 0x00,
    0x41,0x0a,
    0x41,0x02, // Y = 34 (relative to module start)
    0x6a,
    0x41,0x03, // X = 37
    0x6a,
    0x20,0x00,
    0x6d,
    0x20,0x00,
    0x6d,
    0x0b
]);

// Wait for Debugger.enable to be processed and scriptParsed to be sent.
// In d8 this happens when we give it a chance to pump messages.
// Calling WebAssembly.Module should trigger it.
const module = new WebAssembly.Module(bytes);
const instance = new WebAssembly.Instance(module);

if (!wasmScriptId) {
  // Try to find it in scriptIds
  wasmScriptId =
      scriptIds.find(id => id > 3);  // Wasm scripts usually have higher IDs
}

print('Using wasmScriptId: ' + wasmScriptId);

print('Setting breakpoint at Y (offset 34)...');
send(JSON.stringify({
  id: 2,
  method: 'Debugger.setBreakpoint',
  params: {location: {scriptId: wasmScriptId, lineNumber: 0, columnNumber: 34}}
}));

print('Calling f(7)...');
try {
  instance.exports.f(7);
} catch (e) {
  print('Caught: ' + e);
}
print('Finished.');
