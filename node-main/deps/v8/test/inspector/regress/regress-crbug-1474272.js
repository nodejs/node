// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { session, Protocol, contextGroup } = InspectorTest.start('Don\'t crash when pausing in a strict function produced by eval.');

contextGroup.addScript(`
"use strict";
var f = eval(\`
  // Function with one parameter AND using 'arguments' requires context allocation
  // in sloppy mode, but not strict mode.
  (function(message) {
    console.log(arguments.length);
    debugger;
  })\`);
`);

(async () => {
  await Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({ expression: 'f("test")' });
  const { params: { callFrames: [{callFrameId}] } } = await Protocol.Debugger.oncePaused();

  const { result } = await Protocol.Debugger.evaluateOnCallFrame({ callFrameId, expression: 'message' });
  InspectorTest.log('Evaluate f("test"):');
  InspectorTest.logMessage(result.result);

  InspectorTest.completeTest();
})();
