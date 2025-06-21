// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/1222065');

const source = `
function foo(x) {
  return x;
}
`;

InspectorTest.runAsyncTestSuite([
  async function test() {
    session.setupScriptMap();
    await Promise.all([
      Protocol.Runtime.enable(),
      Protocol.Debugger.enable()
    ]);
    contextGroup.addScript(source, 0, 0, 'foo.js');
    const { params: { scriptId } } = await Protocol.Debugger.onceScriptParsed();

    InspectorTest.log('- Debugger.setBreakpoint(foo.js:2:9)');
    let {result: {actualLocation, breakpointId}} = await Protocol.Debugger.setBreakpoint({location: {
      scriptId,
      lineNumber: 2,
      columnNumber: 9,
    }});
    await session.logSourceLocation(actualLocation);
    await Protocol.Debugger.removeBreakpoint({breakpointId});

    InspectorTest.log('- Debugger.setBreakpoint(foo.js:3)');
    ({result: {actualLocation, breakpointId}} = await Protocol.Debugger.setBreakpoint({location: {
      scriptId,
      lineNumber: 3,
    }}));
    await session.logSourceLocation(actualLocation);
    await Protocol.Debugger.removeBreakpoint({breakpointId});

    await Promise.all([
      Protocol.Runtime.disable(),
      Protocol.Debugger.disable()
    ]);
  }
]);
