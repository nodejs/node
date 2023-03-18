// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Regression test for crbug.com/1220203.');

contextGroup.addScript(`
async function *generatorFunction() {
  await 1;
  throwError();
}

function throwError() {
  throw new Error();
}

async function main() {
  for await (const value of generatorFunction()) {}
}`);

session.setupScriptMap();

InspectorTest.runAsyncTestSuite([
  async function testBreakOnUncaughtException() {
    await Promise.all([
      Protocol.Runtime.enable(),
      Protocol.Debugger.enable(),
      Protocol.Debugger.setPauseOnExceptions({state: 'uncaught'}),
    ]);
    const pausedPromise = Protocol.Debugger.oncePaused();
    const evalPromise = Protocol.Runtime.evaluate({expression: 'main()', awaitPromise: true});
    const {params: {callFrames, data}} = await pausedPromise;
    InspectorTest.log(`${data.uncaught ? 'Uncaught' : 'Caught'} exception at`);
    await session.logSourceLocation(callFrames[0].location);

    await Protocol.Debugger.resume();
    // Wait on this before the Promise.all to ensure we didn't break twice (crbug.com/1270780).
    await evalPromise;

    await Promise.all([
      Protocol.Runtime.disable(),
      Protocol.Debugger.disable(),
    ]);
  },
]);
