// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests stepping into microtasks across Promise chains.');

const source = `
function f() {
  console.log(1);
}

function runPromiseChain() {
  return Promise.resolve().then(f).then(
  function() {
    return 2;
  }
  ).then(
  function() {
    throw new Error();
  }
  ).catch(
  function() {
    console.log(3);
  }  // STOP
  );
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  session.setupScriptMap();
  const lines = source.split('\n');
  await Protocol.Debugger.enable();

  await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 2, columnNumber: 0, url: 'test.js'});

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const line = lines[callFrames[0].location.lineNumber];
    InspectorTest.log(line);
    if (!/STOP/.test(line)) {
      await Protocol.Debugger.stepInto();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  await Protocol.Runtime.evaluate(
      {expression: 'runPromiseChain()', awaitPromise: true});

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
