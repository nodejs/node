// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
  'Tests different combinations of async stacks in chains.');

contextGroup.addScript(`
function runWithRegular(f, name) {
  inspector.scheduleWithAsyncStack(f, name, false);
}

function runWithEmptyName(f) {
  inspector.scheduleWithAsyncStack(f, '', false);
}

function runWithEmptyStack(f, name) {
  inspector.scheduleWithAsyncStack(f, name, true);
}

function runWithEmptyNameEmptyStack(f) {
  inspector.scheduleWithAsyncStack(f, '', true);
}

function runWithExternal(f) {
  const id = inspector.storeCurrentStackTrace('external');
  runWithRegular(() => {
    inspector.externalAsyncTaskStarted(id);
    f();
    inspector.externalAsyncTaskFinished(id);
  }, 'not-used-async');
}

function runWithNone(f) {
  f();
}
//# sourceURL=utils.js`);

session.setupScriptMap();
(async function test() {
  Protocol.Debugger.enable();
  Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 128});

  const first = ['Regular', 'EmptyName', 'EmptyStack', 'EmptyNameEmptyStack', 'External']
  const second = ['None', 'Regular', 'EmptyName', 'EmptyStack', 'EmptyNameEmptyStack', 'External']

  for (const stack1 of first) {
    for (const stack2 of second) {
      if (stack1 === 'External' && stack2 !== 'None') continue;

      InspectorTest.log(stack2 === 'None' ? stack1 : `${stack1} - ${stack2}`);
      Protocol.Runtime.evaluate({
        expression: `
          var userFunction = () => {debugger};
          var inner = () => runWith${stack1}(userFunction, 'inner async');
          runWith${stack2}(inner, 'outer async');
          //# sourceURL=test.js`
      });
      await pauseAndDumpStack();
    }
  }

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();

async function pauseAndDumpStack() {
  const {params:{callFrames, asyncStackTrace, asyncStackTraceId}}
      = await Protocol.Debugger.oncePaused();
  session.logCallFrames(callFrames);
  if (asyncStackTrace)
    session.logAsyncStackTrace(asyncStackTrace);
  if (asyncStackTraceId)
    InspectorTest.log('  <external stack>');
  InspectorTest.log('');
  return Protocol.Debugger.resume();
}
