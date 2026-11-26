// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that scope chains are populated on debugger and stepInto.');

contextGroup.addScript(`
var sum = 0;
function runTest() {
  (function (){
    'use strict';

    debugger; // Break 0.
    var i = 0; // Break 1.
    i++; // Break 2.
    i++; // Break 3.
    debugger;  // Break 4.
    return i  // Break 5.
    ; // Break 6.
  }());
} // Break 7.
`);

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();

  let break_count = 0;
  const expected_breaks = 8;

  Protocol.Debugger.onPaused(async ({params}) => {
    InspectorTest.log(`break ${break_count}, scopes: ${
        params.callFrames[0].scopeChain.length}`);
    await session.logSourceLocation(params.callFrames[0].location);
    ++break_count;
    if (break_count !== expected_breaks) {
      InspectorTest.log('Next step prepared');
      Protocol.Debugger.stepInto();
    } else {
      Protocol.Debugger.resume();
    }
  });

  await Protocol.Runtime.evaluate({expression: 'runTest()'});
  InspectorTest.log(`total breaks: ${break_count}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
