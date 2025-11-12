// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks that async stacks works for async/await');

contextGroup.addInlineScript(`
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
}`, 'test.js');

session.setupScriptMap();
Protocol.Debugger.onPaused(message => {
  session.logCallFrames(message.params.callFrames);
  session.logAsyncStackTrace(message.params.asyncStackTrace);
  InspectorTest.log('');
  Protocol.Debugger.resume();
});

Protocol.Debugger.enable();
Protocol.Debugger.setAsyncCallStackDepth({ maxDepth: 128 });
Protocol.Runtime.evaluate({ expression: 'test()//# sourceURL=expr.js',
    awaitPromise: true })
  .then(InspectorTest.logMessage)
  .then(InspectorTest.completeTest);
