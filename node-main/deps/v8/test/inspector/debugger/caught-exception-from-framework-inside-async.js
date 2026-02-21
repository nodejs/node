// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Async caught exception prediction and blackboxing.');

contextGroup.addScript(`
function constructorThrow() {
  return new Promise((resolve, reject) =>
    Promise.resolve().then(() =>
      reject("f")  // Exception f
    )
  );
}

function dotCatch(producer) {
  Promise.resolve(producer()).catch(() => {});
}
//# sourceURL=framework.js`);

session.setupScriptMap();
(async function test() {
  Protocol.Debugger.enable();
  Protocol.Debugger.setBlackboxPatterns({patterns: ['framework\.js']});
  Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  Protocol.Runtime.evaluate({expression: 'dotCatch(constructorThrow);'});
  // Should break at this debugger statement, not at reject.
  Protocol.Runtime.evaluate({expression: 'setTimeout(\'debugger;\', 0);'});
  await waitPauseAndDumpLocation();
  InspectorTest.completeTest();
})();

async function waitPauseAndDumpLocation() {
  var message = await Protocol.Debugger.oncePaused();
  InspectorTest.log('paused at:');
  session.logSourceLocation(message.params.callFrames[0].location);
  return message;
}
