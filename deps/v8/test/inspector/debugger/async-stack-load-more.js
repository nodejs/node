// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests super long async stacks.');

contextGroup.addScript(`
function callWithAsyncStack(f, depth) {
  if (depth === 0) {
    f();
    return;
  }
  wrapper = eval('(function call' + depth + '() { callWithAsyncStack(f, depth - 1) }) //# sourceURL=wrapper.js');
  Promise.resolve().then(wrapper);
}
//# sourceURL=utils.js`);

(async function test() {
  Protocol.Debugger.enable();
  Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 4});
  Protocol.Runtime.evaluate({
    expression: 'callWithAsyncStack(() => {debugger}, 12)//# sourceURL=expr.js'
  });
  let {params} = await Protocol.Debugger.oncePaused();
  let {callFrames, asyncStackTrace, externalAsyncStackTrace} = params;
  while (true) {
    session.logCallFrames(callFrames);
    if (externalAsyncStackTrace) {
      InspectorTest.log('(fetch parent..)');
      asyncStackTrace = (await Protocol.Debugger.getStackTrace({
        stackTraceId: externalAsyncStackTrace
      })).result.stackTrace;
    }
    if (asyncStackTrace) {
      InspectorTest.log('--' + asyncStackTrace.description + '--');
      callFrames = asyncStackTrace.callFrames;
      externalAsyncStackTrace = asyncStackTrace.parentId;
      asyncStackTrace = asyncStackTrace.parent;
    } else {
      break;
    }
  }
  InspectorTest.completeTest();
})()
