// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests promise-finally with break on exception.');

const source = `
var thenable = {
  get then() {
    throw new Error('err');  // Exception
  }
};

async function run() {
  var caughtException = null;
  await Promise.resolve()
    .finally(() => thenable)
    .catch(e => caughtException = e);
  return caughtException !== null;
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});

  let step = 0;
  let exception = null;

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    try {
      let {result: {scriptSource}} = await Protocol.Debugger.getScriptSource(
          {scriptId: callFrames[0].location.scriptId});
      let line = scriptSource.split('\n')[callFrames[0].location.lineNumber];
      let match = /Exception/.exec(line);
      if (!match) throw new Error('No match on line: ' + line);
      step++;
    } catch (e) {
      exception = e;
    }
    await Protocol.Debugger.resume();
  });

  const {result} = await Protocol.Runtime.evaluate(
      {expression: 'run()', awaitPromise: true});
  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});

  InspectorTest.log(`step: ${step}`);
  InspectorTest.log(`exception: ${exception}`);
  InspectorTest.log(`caughtException: ${result.result.value}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
