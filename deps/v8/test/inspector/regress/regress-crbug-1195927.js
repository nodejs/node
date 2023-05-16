// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/1195927');

const source = `
function foo(x) {
  x = x + 1;
  return x;
}
`;

const newSource = `
function foo(x) {
  x = x + 2;
  return x;
}
`;

InspectorTest.runAsyncTestSuite([
  async function test() {
    session.setupScriptMap();
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    contextGroup.addScript(source, 0, 0, 'foo.js');
    const { params: { scriptId } } = await Protocol.Debugger.onceScriptParsed();

    let {result} = await Protocol.Debugger.setBreakpoint({location: {
      scriptId,
      lineNumber: 2,
    }});
    InspectorTest.log('Debugger.setBreakpoint result:');
    InspectorTest.logMessage(result);

    const callPromise = Protocol.Runtime.evaluate({expression: 'foo(42)'});

    let {params: {callFrames}} = await Protocol.Debugger.oncePaused();
    InspectorTest.log('Debugger.paused call frames:');
    session.logCallFrames(callFrames);

    ({result} = await Protocol.Debugger.setScriptSource({
      scriptId,
      scriptSource: newSource
    }));
    InspectorTest.log('Debugger.setScriptSource result:');
    InspectorTest.logMessage(result);

    ([, {result}] = await Promise.all([Protocol.Debugger.resume(), callPromise]));
    InspectorTest.log('foo(42) result:');
    InspectorTest.logMessage(result);
  }
]);
