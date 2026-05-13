// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector --wasm-lazy-validation --wasm-lazy-compilation

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let wasmScriptId = null;

globalThis.receive = function(msg) {
  let m = JSON.parse(msg);
  if (m.method === 'Debugger.scriptParsed' &&
      m.params.scriptLanguage === 'WebAssembly') {
    wasmScriptId = m.params.scriptId;
  }
};

send('{"id": 1, "method": "Debugger.enable"}');

var poc_buffer = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x04,                    // section length 4
  0x01, 0x60,              // types count 1: kind: func
  0x00,                    // param count 0
  0x00,                    // return count 0

  0x03,                    // section kind: Function
  0x02,                    // section length 2
  0x01, 0x00,              // functions count 1: 0 $main

  0x07,                    // section kind: Export
  0x08,                    // section length 8
  0x01,                    // exports count 1: export # 0
  0x04,                    // field name length: 4
  0x6d, 0x61, 0x69, 0x6e,  // field name: main
  0x00, 0x00,              // kind: function index: 0

  0x0a,                    // section kind: Code
  0x0c,                    // section length 12
  0x01,                    // functions count 1
                           // function #0 $main
  0x0a,                    // body size 10
  0x02,                    // local decls count 2
  0xff, 0xff, 0xff, 0xff, 0x0f,  // local count 4294967295
  0x7f,                    // i32
  0x02,                    // local count 2
  0x7f,                    // i32
  0x0b                     // end
]);

var mod = new WebAssembly.Module(poc_buffer);
var inst = new WebAssembly.Instance(mod);

assertNotNull(wasmScriptId, 'WASM script not found');

send(JSON.stringify({
  id: 2,
  method: 'Debugger.getPossibleBreakpoints',
  params: {
    start: {scriptId: wasmScriptId, lineNumber: 0, columnNumber: 0},
    end: {scriptId: wasmScriptId, lineNumber: 0, columnNumber: 1000}
  }
}));
