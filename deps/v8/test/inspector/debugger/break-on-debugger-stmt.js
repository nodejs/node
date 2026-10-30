// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests break-on-debugger-stmt.');

contextGroup.addScript(`
function f() { debugger; debugger; }
`);

(async function test() {
  await Protocol.Debugger.enable();
  let breakEventCount = 0;
  Protocol.Debugger.onPaused(() => {
    breakEventCount++;
    Protocol.Debugger.resume();
  });

  InspectorTest.log('initial breakEventCount: ' + breakEventCount);
  await Protocol.Runtime.evaluate({expression: 'f(); f(); f();'});
  InspectorTest.log('final breakEventCount: ' + breakEventCount);
  InspectorTest.completeTest();
})();
