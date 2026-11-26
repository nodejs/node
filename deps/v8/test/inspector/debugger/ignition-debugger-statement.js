// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests ignition-debugger-statement.');

contextGroup.addScript(`
function f() {
  debugger;
}
`);

(async function test() {
  let breakCount = 0;
  Protocol.Debugger.onPaused(() => {
    breakCount++;
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log('before enable: ' + breakCount);

  await Protocol.Debugger.enable();
  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log('after enable: ' + breakCount);

  await Protocol.Debugger.disable();
  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log('after disable: ' + breakCount);

  InspectorTest.completeTest();
})();
