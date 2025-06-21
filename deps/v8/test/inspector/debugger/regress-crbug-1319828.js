// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/1319828');

const source = `
function foo() {
  console.log('Hello World!');
}
//# sourceURL=foo.js`;
contextGroup.addScript(source, 42, 3);
session.setupScriptMap();

InspectorTest.runAsyncTestSuite([
  async function testDebuggerGetPossibleBreakpoints() {
    const [{params: {scriptId}}] = await Promise.all([
      Protocol.Debugger.onceScriptParsed(),
      Protocol.Runtime.enable(),
      Protocol.Debugger.enable(),
    ]);
    const {result: {locations}} =
        await Protocol.Debugger.getPossibleBreakpoints({
          start: {lineNumber: 1, columnNumber: 0, scriptId},
          end: {lineNumber: 3, columnNumber: 1, scriptId},
        });
    await session.logBreakLocations(locations);
    await Promise.all([
      Protocol.Debugger.disable(),
      Protocol.Runtime.disable(),
    ]);
  },

  async function testDebuggerSetBreakpointByUrl() {
    await Promise.all([
      Protocol.Runtime.enable(),
      Protocol.Debugger.enable(),
    ]);
    let {result: {breakpointId, locations}} =
        await Protocol.Debugger.setBreakpointByUrl({
          url: 'foo.js',
          lineNumber: 2,
          columnNumber: 0,
        });
    await Promise.all([
      session.logSourceLocations(locations),
      Protocol.Debugger.removeBreakpoint({breakpointId}),
    ]);
    ({result: {breakpointId, locations}} =
         await Protocol.Debugger.setBreakpointByUrl({
           url: 'foo.js',
           lineNumber: 2,
           columnNumber: 9,
         }));
    await Promise.all([
      session.logSourceLocations(locations),
      Protocol.Debugger.removeBreakpoint({breakpointId}),
    ]);
    await Promise.all([
      Protocol.Debugger.disable(),
      Protocol.Runtime.disable(),
    ]);
  },
]);
