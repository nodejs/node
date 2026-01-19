// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
  InspectorTest.start('Checks that restart frame fails for generator or async functions.');

session.setupScriptMap();

async function testCase(description, snippet, restartFrameIndex) {
  InspectorTest.log('');
  InspectorTest.log(description);

  const { callFrames, evaluatePromise } = await InspectorTest.evaluateAndWaitForPause(snippet);
  // These are negative tests where the following call is expected to fail.
  await InspectorTest.restartFrameAndWaitForPause(callFrames, restartFrameIndex, /*quitOnFailure*/ false);

  // All snippets are written so a single resume is enough.
  Protocol.Debugger.resume();
  await evaluatePromise;
}

(async () => {
  await Protocol.Debugger.enable();

  await testCase('Check that an async function cannot be restarted.', `
    (async function asyncFn() {
      debugger;
    })();
  `, 0);

  await testCase('Check that a generator function cannot be restarted.', `
    function* generatorFn() {
      yield 10;
      debugger;
      yield 20;
    }
    const gen1 = generatorFn();
    gen1.next();
    gen1.next();
  `, 0);

  await testCase('Check that a function cannot be restarted when a generator function is on the stack above', `
    function breaker() { debugger; }
    function* genFn() {
      yield 10;
      breaker();
      yield 20;
    }
    const gen2 = genFn();
    function bar() {  // We want to restart bar.
      gen2.next();
      gen2.next();
    }
    bar();
  `, 2);

  InspectorTest.completeTest();
})();
