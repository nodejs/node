// Copyright 2021 the V8 project authors. All rights reserved.
// Use  of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Async generator stepping');

const url = 'stepping-async-generator.js';
contextGroup.addScript(`
async function* generator() {
  var a = 42;
  yield a;
}
function callGenerator() {
  return generator();
}
`, 0, 0, url);

session.setupScriptMap();

InspectorTest.runAsyncTestSuite([
  async function testStepIntoInitialYield() {
    await Promise.all([Protocol.Debugger.enable(), Protocol.Runtime.enable()]);
    InspectorTest.log(`Setting breakpoint on call to generator()`);
    const {result: {breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
      url,
      lineNumber: 5,
      columnNumber: 0,
    })
    InspectorTest.log(`Calling callGenerator()`);
    const pausedPromise = Protocol.Debugger.oncePaused();
    const evalPromise = Protocol.Runtime.evaluate({expression: 'callGenerator()'});
    const {method, params} = await Promise.race([pausedPromise, evalPromise]);
    if (method === 'Debugger.paused') {
      await session.logSourceLocation(params.callFrames[0].location);

      InspectorTest.log('Stepping into the generator()');
      let [{params: {callFrames:[{location}]}}] = await Promise.all([
        Protocol.Debugger.oncePaused(),
        Protocol.Debugger.stepInto(),
      ]);
      await session.logSourceLocation(location);

      await Promise.all([Protocol.Debugger.resume(), evalPromise]);
    } else {
      InspectorTest.log('Did not pause');
    }
    await Protocol.Debugger.removeBreakpoint({breakpointId});
    await Promise.all([Protocol.Debugger.disable(), Protocol.Runtime.disable()]);
  }
]);
