// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping over at the end of a script.');

const source = 'debugger;\n';
const lines = source.split('\n');

Protocol.Debugger.onPaused(async message => {
  const line = message.params.callFrames[0].location.lineNumber;
  InspectorTest.log(`Paused at: "${lines[line]}"`);
  await Protocol.Debugger.stepOver();
});

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.evaluate({expression: source});
  InspectorTest.completeTest();
})();
