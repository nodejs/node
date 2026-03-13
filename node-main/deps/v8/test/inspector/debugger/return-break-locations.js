// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Return break locations within function');

contextGroup.addScript(`
function fib(x) {
  if (x < 0) return;
  if (x === 0) return 1;
  if (x === 1) return fib(0);
  return x > 2 ? fib(x - 1) + fib(x - 2) : fib(1) + fib(0);
}
`);

InspectorTest.runAsyncTestSuite([
  async function testTailCall() {
    var scriptPromise = Protocol.Debugger.onceScriptParsed();
    Protocol.Debugger.enable();
    var scriptId = (await scriptPromise).params.scriptId;
    var locations = (await Protocol.Debugger.getPossibleBreakpoints({
      start: { lineNumber: 0, columnNumber: 0, scriptId }
    })).result.locations;
    InspectorTest.logMessage(locations.filter(location => location.type === 'return'));
  }
]);
