// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --expose-gc

let {session, contextGroup, Protocol} = InspectorTest.start('Checks async stack for late .then handlers with gc');

contextGroup.addInlineScript(
    `
function foo1() {
  gc();
  debugger;
}

function test() {
  var resolve1;
  var p1 = new Promise(resolve => resolve1 = resolve);
  gc();
  var p2 = p1.then(foo1);
  gc();
  resolve1();
  gc();
  var p3 = p1.then(foo1);
  gc();
  var p4 = p1.then(foo1);
  gc();
  return Promise.all([p2,p3,p4]);
}`,
    'test.js');

session.setupScriptMap();
Protocol.Debugger.onPaused(message => {
  session.logCallFrames(message.params.callFrames);
  var asyncStackTrace = message.params.asyncStackTrace;
  while (asyncStackTrace) {
    InspectorTest.log(`-- ${asyncStackTrace.description} --`);
    session.logCallFrames(asyncStackTrace.callFrames);
    asyncStackTrace = asyncStackTrace.parent;
  }
  InspectorTest.log('');
  Protocol.Debugger.resume();
});

Protocol.Debugger.enable();
Protocol.Debugger.setAsyncCallStackDepth({ maxDepth: 128 });
Protocol.Runtime.evaluate({ expression: 'test()//# sourceURL=expr.js',
    awaitPromise: true })
  .then(() => Protocol.Runtime.evaluate({ expression: 'gc()'}))
  .then(InspectorTest.completeTest);
