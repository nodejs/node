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
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x06,                    // section length 6
  0x01, 0x60,              // types count 1:  kind: func
  0x01, 0x7f,              // param count 1:  i32
  0x01, 0x7f,              // return count 1:  i32

  0x03,                    // section kind: Function
  0x02,                    // section length 2
  0x01, 0x00,              // functions count 1: 0 $f (param i32) (result i32)

  0x07,                    // section kind: Export
  0x05,                    // section length 5
  0x01,                    // exports count 1: export # 0
  0x01,                    // field name length:  1
  0x66,                    // field name: f
  0x00, 0x00,              // kind: function index:  0

  0x0a,                    // section kind: Code
  0x12,                    // section length 18
  0x01,                    // functions count 1
                           // function #0 $f
  0x10,                    // body size 16
  0x00,                    // 0 entries in locals list
  0x41, 0x0a,              // i32.const 10
  0x41, 0x02,              // i32.const 2  <-- Y = 34 (relative to module start)
  0x6a,                    // i32.add
  0x41, 0x03,              // i32.const 3  <-- X = 37
  0x6a,                    // i32.add
  0x20, 0x00,              // local.get $var0
  0x6d,                    // i32.div_s
  0x20, 0x00,              // local.get $var0
  0x6d,                    // i32.div_s
  0x0b,                    // end
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
