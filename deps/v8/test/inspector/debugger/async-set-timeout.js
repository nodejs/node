// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks that async stack contains setTimeout');

contextGroup.addScript(`
var resolveCallback;
function foo1() {
  function inner1() {
    debugger;
    resolveCallback();
  }
  inner1();
}
function foo2() {
  function inner2() {
    setTimeout(foo1, 0);
  }
  inner2();
}
function foo3() {
  var promise = new Promise(resolve => resolveCallback = resolve);
  function inner3() {
    setTimeout(foo2, 0);
  }
  inner3();
  return promise;
}
//# sourceURL=test.js`, 7, 26);

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
Protocol.Runtime.evaluate({ expression: "foo3()//# sourceURL=expr.js", awaitPromise: true })
  .then(InspectorTest.completeTest);
