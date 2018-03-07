// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
  'Tests breakpoint when two scripts have the same url.');

// Order of addScript is important!
contextGroup.addScript(`
function boo() {
  return 42;
}
function foo() {}
`, 0, 0, 'test.js');

contextGroup.addScript(`function foo() {}`, 15, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  let {result} = await Protocol.Debugger.setBreakpointByUrl({
    url: 'test.js',
    lineNumber: 2,
    columnNumber: 2
  });
  InspectorTest.logMessage(result);
  InspectorTest.completeTest();
})();
