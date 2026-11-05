// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignore-unhandled-promises

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-568477-3.');

contextGroup.addScript(`
function f() {
  var a = 1;
  debugger;
  var x = y;
  return x;
}

function call_f_with_deeper_stack() {
  (() => () => () => f())()()();
}
`);

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async message => {
    const location = message.params.callFrames[0].location;
    await session.logSourceLocation(location);
    if (location.lineNumber === 3) {
      Protocol.Debugger.stepOver();
    } else {
      Protocol.Debugger.resume();
    }
  });

  await Protocol.Runtime.evaluate({
    expression:
        'var p = Promise.resolve();\np.then(f);\np.then(call_f_with_deeper_stack);'
  });
  InspectorTest.completeTest();
})();
