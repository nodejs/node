// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that a second breakpoint condition at the same location is not ' +
    'evaluated if the first condition synchronously disables the debugger.');

const source = `
function outer() {
  function inner1() { return 1; }
  function inner2() { return 2; }
  return inner1() + inner2();
}
let cond1Evaluated = false;
let cond2Evaluated = false;
globalThis.cond1 = function() {
  cond1Evaluated = true;
  inspector.disconnectSession(${session.id});
  return false;
};
globalThis.cond2 = function() {
  cond2Evaluated = true;
  return false;
};
`;

(async function test() {
  await Protocol.Debugger.enable();
  contextGroup.addScript(source, 0, 0, 'test.js');
  const {params: {scriptId}} = await Protocol.Debugger.onceScriptParsed();

  await Protocol.Debugger.setBreakpoint({
    location: {scriptId, lineNumber: 2, columnNumber: 22},
    condition: 'globalThis.cond1()',
  });
  await Protocol.Debugger.setBreakpointByUrl({
    urlRegex: 'test\\.js',
    lineNumber: 2,
    columnNumber: 22,
    condition: 'globalThis.cond2()',
  });

  contextGroup.addScript('globalThis.outerResult = outer();');
  InspectorTest._sessions.delete(session);

  const session2 = contextGroup.connect();
  const {result: {result}} = await session2.Protocol.Runtime.evaluate({
    expression: '({outerResult, cond1Evaluated, cond2Evaluated})',
    returnByValue: true,
  });
  InspectorTest.logObject(result.value);
  InspectorTest.completeTest();
})();
