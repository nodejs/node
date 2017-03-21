// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

print('Checks that async stacks works for async/await');

InspectorTest.addScript(`
async function foo1() {
  debugger;
  return Promise.resolve();
}

async function foo2() {
  await Promise.resolve();
  debugger;
  await Promise.resolve();
  debugger;
  await foo1();
  await Promise.all([ Promise.resolve() ]).then(foo1);
  debugger;
}

async function test() {
  await foo2();
}
//# sourceURL=test.js`, 7, 26);

InspectorTest.setupScriptMap();
Protocol.Debugger.onPaused(message => {
  InspectorTest.logCallFrames(message.params.callFrames);
  var asyncStackTrace = message.params.asyncStackTrace;
  while (asyncStackTrace) {
    InspectorTest.log(`-- ${asyncStackTrace.description} --`);
    InspectorTest.logCallFrames(asyncStackTrace.callFrames);
    asyncStackTrace = asyncStackTrace.parent;
  }
  InspectorTest.log('');
  Protocol.Debugger.resume();
});

Protocol.Debugger.enable();
Protocol.Debugger.setAsyncCallStackDepth({ maxDepth: 128 });
Protocol.Runtime.evaluate({ expression: 'test()//# sourceURL=expr.js',
    awaitPromise: true })
  .then(InspectorTest.logMessage)
  .then(InspectorTest.completeTest);
