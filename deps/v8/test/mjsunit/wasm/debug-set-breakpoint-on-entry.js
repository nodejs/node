// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector

let breakpoint_resolved = false;

globalThis.receive = (msg) => {
  let m = JSON.parse(msg);
  if (m.method === 'Debugger.scriptParsed' && m.params.url.startsWith('wasm://')) {
    send(JSON.stringify({
      id: 2,
      method: 'Debugger.setBreakpoint',
      params: {
        location: {
          scriptId: m.params.scriptId,
          lineNumber: 0,
          columnNumber: -1 // -1 is not a valid regular breakpoint position.
        }
      }
    }));
  }
  if (m.id === 2) {
    breakpoint_resolved = true;
    assertTrue(!!m.error, 'Breakpoint at -1 should fail');
    assertEquals('Could not resolve breakpoint', m.error.message);
  }
};

send(JSON.stringify({id: 1, method: 'Debugger.enable'}));
new WebAssembly.Module(new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0]));

assertTrue(breakpoint_resolved);
