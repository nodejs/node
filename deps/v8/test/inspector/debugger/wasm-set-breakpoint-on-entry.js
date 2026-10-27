// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests setting a Wasm breakpoint at columnNumber -1.');

(async () => {
  await Protocol.Debugger.enable();
  const scriptParsedPromise = Protocol.Debugger.onceScriptParsed(2);
  await Protocol.Runtime.evaluate({
    expression:
        'new WebAssembly.Module(new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0]));',
  });
  const [, {params: {scriptId}}] = await scriptParsedPromise;
  InspectorTest.logMessage(await Protocol.Debugger.setBreakpoint({
    location: {
      scriptId,
      lineNumber: 0,
      columnNumber: -1,  // -1 is not a valid regular breakpoint position.
    },
  }));
  InspectorTest.completeTest();
})();
