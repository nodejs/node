// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

const {Protocol} = InspectorTest.start(
    'Tests that Debugger.setBreakpointByUrl can compile and match regexes ' +
    'inside an API interrupt when --expose-gc is enabled.');

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({
    expression: `
      while (true) {}
    //# sourceURL=loop.js`
  });
  const bpPromise = Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 1,
    urlRegex: '(loop)\\.js$',
  });
  utils.interruptForMessages();
  InspectorTest.logMessage(await bpPromise);
  InspectorTest.completeTest();
})();
