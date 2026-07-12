// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('stepOut async function');

session.setupScriptMap();

Protocol.Runtime.enable();

InspectorTest.runAsyncTestSuite([
  async function testTrivial() {
    InspectorTest.log('Check that we have proper async stack at return');
    contextGroup.addInlineScript(`
      async function test() {
        await Promise.resolve();
        await foo();
      }

      async function foo() {
        await Promise.resolve();
        await bar();
      }

      async function bar() {
        await Promise.resolve();
        debugger;
    }`, 'testTrivial.js');
    await runTestAndStepAction('stepOut');
  },

  async function testStepOutPrecision() {
    InspectorTest.log('Check that stepOut go to resumed outer generator');
    contextGroup.addInlineScript(`
      function wait() {
        return new Promise(resolve => setTimeout(resolve, 0));
      }
      function floodWithTimeouts(a) {
        if (!a.stop)
          setTimeout(floodWithTimeouts.bind(this, a), 0);
      }

      async function test() {
        let a = {};
        floodWithTimeouts(a)
        await wait();
        await foo();
        await wait();
        a.stop = true;
      }

      async function foo() {
        await Promise.resolve();
        await bar();
        await wait();
      }

      async function bar() {
        await Promise.resolve();
        debugger;
        await wait();
      }`, 'testStepOutPrecision.js');
    await runTestAndStepAction('stepOut');
  },

  async function testStepIntoAtReturn() {
    InspectorTest.log('Check that stepInto at return go to resumed outer generator');
    contextGroup.addInlineScript(`
      function wait() {
        return new Promise(resolve => setTimeout(resolve, 0));
      }
      function floodWithTimeouts(a) {
        if (!a.stop)
          setTimeout(floodWithTimeouts.bind(this, a), 0);
      }

      async function test() {
        let a = {};
        floodWithTimeouts(a)
        await wait();
        await foo();
        a.stop = true;
      }

      async function foo() {
        await Promise.resolve();
        await bar();
      }

      async function bar() {
        await Promise.resolve();
        debugger;
      }`, 'testStepIntoAtReturn.js');
    await runTestAndStepAction('stepInto');
  },

  async function testStepOverAtReturn() {
    InspectorTest.log('Check that stepOver at return go to resumed outer generator');
    contextGroup.addInlineScript(`
      function wait() {
        return new Promise(resolve => setTimeout(resolve, 0));
      }
      function floodWithTimeouts(a) {
        if (!a.stop)
          setTimeout(floodWithTimeouts.bind(this, a), 0);
      }

      async function test() {
        let a = {};
        floodWithTimeouts(a)
        await wait();
        await foo();
        a.stop = true;
      }

      async function foo() {
        await Promise.resolve();
        await bar();
      }

      async function bar() {
        await Promise.resolve();
        debugger;
      }`, 'testStepIntoAtReturn.js');
    await runTestAndStepAction('stepOver');
  },

  async function testStepOutFromNotAwaitedCall() {
    InspectorTest.log('Checks stepOut from not awaited call');
    contextGroup.addInlineScript(`
      function wait() {
        return new Promise(resolve => setTimeout(resolve, 0));
      }
      function floodWithTimeouts(a) {
        if (!a.stop)
          setTimeout(floodWithTimeouts.bind(this, a), 0);
      }

      async function test() {
        let a = {};
        floodWithTimeouts(a)
        await wait();
        await foo();
        a.stop = true;
      }

      async function foo() {
        let a = {};
        floodWithTimeouts(a);
        await Promise.resolve();
        bar();
        a.stop = true;
      }

      async function bar() {
        await Promise.resolve();
        debugger;
      }`, 'testStepIntoAtReturn.js');
    await runTestAndStepAction('stepOut');
  }

]);

async function runTestAndStepAction(action) {
  Protocol.Debugger.enable();
  Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 128});
  let finished =
      Protocol.Runtime.evaluate({expression: 'test()', awaitPromise: true})
          .then(() => false);
  while (true) {
    const r = await Promise.race([finished, waitPauseAndDumpStack()]);
    if (!r) break;
    Protocol.Debugger[action]();
  }
  await Protocol.Debugger.disable();
}

async function waitPauseAndDumpStack() {
  const {params} = await Protocol.Debugger.oncePaused();
  session.logCallFrames(params.callFrames);
  session.logAsyncStackTrace(params.asyncStackTrace);
  InspectorTest.log('');
  return true;
}
