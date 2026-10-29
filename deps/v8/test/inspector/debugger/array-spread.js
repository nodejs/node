// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests breakable locations in array spread.');

const source = `
function testFunction() {
  var a = [...iterable];
  var b = [...a, ...iterable, ...a];
}

const iterable = {
  [Symbol.iterator]() {
    const it = [1, 2].values();
    return {next() { return it.next(); }};
  }
};
`;

const url = 'test.js';
contextGroup.addScript(source, 0, 0, url);
session.setupScriptMap();

InspectorTest.runAsyncTestSuite([
  async function testBreakLocations() {
    let [, , {params: {scriptId}}] = await Promise.all([
      Protocol.Runtime.enable(),
      Protocol.Debugger.enable(),
      Protocol.Debugger.onceScriptParsed(),
    ]);
    let {result: {locations}} = await Protocol.Debugger.getPossibleBreakpoints(
        {start: {lineNumber: 0, columnNumber: 0, scriptId}});
    await session.logBreakLocations(locations);
    await Promise.all([
      Protocol.Debugger.disable(),
      Protocol.Runtime.disable(),
    ]);
  },

  async function testStepping() {
    let [, , {params: {scriptId}}] = await Promise.all([
      Protocol.Runtime.enable(),
      Protocol.Debugger.enable(),
      Protocol.Debugger.onceScriptParsed(),
    ]);
    const {breakpointId} = await Protocol.Debugger.setBreakpoint({
      location: {
        scriptId,
        lineNumber: 2,
      }
    });
    const evalPromise =
        Protocol.Runtime.evaluate({expression: 'testFunction()'});
    for (;;) {
      const {method, params} = await Promise.race([
        evalPromise,
        Protocol.Debugger.oncePaused(),
      ]);
      if (method !== 'Debugger.paused') {
        break;
      }
      const callFrames = params.callFrames.filter(
          callFrame => callFrame.location.scriptId === scriptId);
      if (callFrames.length === 0) {
        InspectorTest.log('Resuming and finishing...');
        await Protocol.Debugger.resume();
      } else {
        const [{functionName, location}, ...callerFrames] = callFrames;
        InspectorTest.log(`Execution paused in ${functionName}:`);
        await session.logSourceLocation(location);
        for (const {location, functionName} of callerFrames) {
          InspectorTest.log(`Called from ${functionName}:`);
          await session.logSourceLocation(location);
        }
        if (functionName === 'testFunction') {
          await Protocol.Debugger.stepInto();
        } else {
          await Protocol.Debugger.stepOut();
        }
      }
    }
    await Promise.all([
      Protocol.Debugger.removeBreakpoint({breakpointId}),
      Protocol.Debugger.disable(),
      Protocol.Runtime.disable(),
    ]);
  }
]);
